#pragma once
#include <queue>
#include <coroutine>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <cassert>
#include <memory>
#include <type_traits>
#include "itembase.h"

// Priority enum for store events
enum class Priority { Low = 0, High = 1 };


struct AllOfEvent;
struct CompareSimEvent;
struct CoroutineProcess;
class SimEvent;
class Task;
// Forward declarations for container event types
struct ContainerPutEvent;
struct ContainerGetEvent;
constexpr bool DEBUG_PRINT_QUEUE = false;
constexpr bool DEBUG_RESOURCE = false;
constexpr bool DEBUG_MEMORY = false;



struct SimEventBase {
    int sim_time;
    int delay = 0;
    size_t unique_id;
    static inline std::atomic<size_t> uid_gen{0};
    static inline std::atomic<size_t> alloc_counter{0};
    static inline std::unordered_map<void*, size_t> alloc_map;
    SimEventBase() : unique_id(++uid_gen) {}
    virtual void resume() = 0;
    virtual ~SimEventBase() = default;
    void* operator new(std::size_t sz) {
        void* p = std::malloc(sz);
        size_t id = ++alloc_counter;
        alloc_map[p] = id;
        if constexpr (DEBUG_MEMORY) {
            std::cout << "[ALLOC] id=" << id << " size=" << sz << " bytes\n";
        }
        return p;
    }
    void operator delete(void* p) noexcept {
        auto it = alloc_map.find(p);
        size_t id = (it != alloc_map.end()) ? it->second : 0;
        if constexpr (DEBUG_MEMORY) {
            std::cout << "[FREE] id=" << id << "\n";
        }
        alloc_map.erase(p);
        std::free(p);
    }
};


struct CompareSimEvent {
    bool operator()(const SimEventBase* a, const SimEventBase* b) {
        if (a->sim_time != b->sim_time)
            return a->sim_time > b->sim_time; // later time loses
        // tie break: use unique_id so older (smaller) wins
        return a->unique_id > b->unique_id;
    }
};
class CSimpyEnv {
public:
    int sim_time = 0;

    std::priority_queue<SimEventBase*, std::vector<SimEventBase*>, CompareSimEvent> event_queue;
    std::vector<std::shared_ptr<Task>> active_tasks;
    std::vector<std::shared_ptr<void>> active_functors;
    void schedule(SimEventBase*);
    void schedule(std::shared_ptr<Task> t, const std::string& label) ;
    template<typename F>
    std::shared_ptr<Task> create_task(F&& coroutine_func);
    void print_event_queue_state();
    void run();
};



// Concrete event for coroutine handles
struct CoroutineProcess : SimEventBase {
    std::coroutine_handle<> handle;
    std::string label;

    CoroutineProcess(int t, std::coroutine_handle<> h, std::string lbl)
        : handle(h), label(std::move(lbl)) {
        sim_time = t;
    }
    void resume() override {
        handle.resume();
    }
};

struct SimEvent : SimEventBase {
    std::shared_ptr<ItemBase> value;
    CSimpyEnv& env;
    std::vector<std::function<void(int)>> callbacks;
    bool done = false;
    // Optional filter for Store get events
    std::function<bool(const std::shared_ptr<ItemBase>&)> item_filter;

    SimEvent(CSimpyEnv& env_) : env(env_) {
        sim_time = env.sim_time + delay;
    }

    bool await_ready() const noexcept {
        if (!done) {
            return false;
        }
        return true;
    }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        callbacks.emplace_back([this, h, label](int time) {
            env.schedule(new CoroutineProcess(time, h, "SimEvent::callback -> " + label));
        });
    }

    auto await_resume() const {
        return value;
    }

    template<typename T>
    void set_value(T val) {
        static_assert(std::is_convertible_v<T, std::shared_ptr<ItemBase>>,
                      "Unsupported type for set_value");
        value = std::move(val);
    }

    void trigger() {
        for (auto& cb : callbacks) {
            cb(env.sim_time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger();
    }

    virtual SimEvent* clone_for_schedule() const {
        auto* clone = new SimEvent(env);
        clone->value = value;
        clone->done = done;
        return clone;
    }

    // Schedules a heap-allocated copy of this event at the given time and clears callbacks.
    void on_succeed() {
        done = true;
        SimEvent* heap_event = this->clone_for_schedule();
        heap_event->callbacks = std::move(this->callbacks);
        //heap_event->sim_time = this->sim_time;
        this->callbacks.clear();
        env.schedule(heap_event);
    }
};




struct TaskPromise {
    std::shared_ptr<SimEvent> completion_event;

    Task get_return_object();

    void set_completion_event(std::shared_ptr<SimEvent> ce) {
        completion_event = std::move(ce);
    }

    std::suspend_always initial_suspend() { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            TaskPromise* promise;

            bool await_ready() noexcept { return false; }
           void await_suspend(std::coroutine_handle<>) const noexcept {
                if (promise->completion_event) {
                    // Signal completion with a FinishItem
                    auto finish_item = std::make_shared<FinishItem>();
                    promise->completion_event->set_value(finish_item);
                    promise->completion_event->on_succeed();
                }
            }

            void await_resume() noexcept {}
        };
        return FinalAwaiter{this};
    }

    void return_void() {}
    void unhandled_exception() { std::exit(1); }
};

struct Task {
    using promise_type = TaskPromise;
    std::coroutine_handle<TaskPromise> h;

    explicit Task(std::coroutine_handle<TaskPromise> h) : h(h) {}

    Task(Task&& other) noexcept : h(other.h) {
        other.h = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (h) h.destroy();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }

    ~Task() {
        if (h) {
            h.destroy();
        }// only destroys if we still own the handle
    }

    // disable copy to avoid double free
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    SimEvent& get_completion_event() {
        return *h.promise().completion_event;
    }

    CSimpyEnv& get_env() {
        return h.promise().completion_event->env;
    }

    bool done() const {
        return h.done();
    }
};





struct LabeledAwait {
    SimEvent& event;
    std::string label;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { event.await_suspend(h, label); }
    auto await_resume() { return event.await_resume(); }
};

struct SimDelay : SimEvent {
    int delay;

    SimDelay(CSimpyEnv& e, int d)
        : SimEvent(e) {
        delay = d;
        sim_time = env.sim_time + delay;
    }

    SimEvent* clone_for_schedule() const override {
        auto* clone = new SimDelay(env, delay);
        clone->done = done;
        return clone;
    }

    void await_suspend(std::coroutine_handle<> h,
                       const std::string& label = "?") {
        callbacks.emplace_back([this, h, label](int when) {
            env.schedule(new CoroutineProcess(
                when, h, "SimDelay::resume handler -> " + label));
        });

        this->on_succeed();


    }

    void resume() override {
        if constexpr (DEBUG_PRINT_QUEUE) {
            std::cout << "[" << env.sim_time << "] SimDelay resumed.\n";
        }
        trigger();
    }
};
// Waits for all of a set of SimEvents to complete, then triggers itself.
struct AllOfEvent : SimEventBase {
    std::vector<SimEvent*> events;
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    int completed = 0;
    CSimpyEnv& env;

    AllOfEvent(CSimpyEnv& env_, std::vector<SimEvent*> evts) : events(std::move(evts)), env(env_) {
        // Removed lambda trampoline and heap-allocated fake awaiters from constructor
        auto x = 3;
    }

    void count(int time) {
        ++completed;
        if (completed == static_cast<int>(events.size())) {
            // Schedule this AllOfEvent itself (heap-allocated)
            auto self = new AllOfEvent(env, events);  // note: shallow copy of `events`
            self->waiters = std::move(waiters);       // transfer ownership of waiters
            self->sim_time = time;                    // assign the current time
            env.schedule(self);
        }
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        waiters.emplace_back(h, label);
        for (SimEvent* e : events) {

            e->callbacks.emplace_back([this](int t) {
                this->count(t);
            });
            if (e->done && dynamic_cast<SimDelay*>(e) == nullptr) {
                // it still has callback to allof event and is not a SimDelay
                e->on_succeed();
            }

            if (auto* delay = dynamic_cast<SimDelay*>(e)) {

                delay->on_succeed();

            }
        }
    }

    auto await_resume() const {
        return std::string("all_done");
    }

    void resume() override {
        for (const auto& [wh, label] : waiters) {
            env.schedule(new CoroutineProcess(env.sim_time, wh, "AllOfEvent::resume handler-> " + label));
        }
        waiters.clear();

    }
};


// Waits for *any* of a set of SimEvents to complete, then triggers itself.
struct AnyOfEvent : SimEventBase {
    std::vector<SimEvent*> events;
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    bool triggered = false;
    CSimpyEnv& env;

    AnyOfEvent(CSimpyEnv& env_, std::vector<SimEvent*> evts)
        : events(std::move(evts)), env(env_) {}

    void trigger_now(int time) {
        if (triggered) return; // prevent double trigger
        triggered = true;

        // Remove all callbacks from other events so they won't fire later
        for (auto* e : events) {
            e->callbacks.clear();
        }

        // Schedule this AnyOfEvent
        auto self = new AnyOfEvent(env, events);
        self->waiters = std::move(waiters);
        self->sim_time = time;
        env.schedule(self);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        waiters.emplace_back(h, label);

        for (SimEvent* e : events) {
            e->callbacks.emplace_back([this](int t) {
                this->trigger_now(t);
            });

            // Handle events that are already done
            if (e->done && dynamic_cast<SimDelay*>(e) == nullptr) {
                // it still has callback to anyof event and is not a SimDelay
                e->on_succeed();
            }

            if (auto* delay = dynamic_cast<SimDelay*>(e)) {

                delay->on_succeed();

            }
        }
    }

    auto await_resume() const {
        return std::string("any_done");
    }

    void resume() override {
        for (const auto& [wh, label] : waiters) {
            env.schedule(new CoroutineProcess(env.sim_time, wh, "AnyOfEvent::resume handler-> " + label));
        }
        waiters.clear();
    }
};

template<typename F>
std::shared_ptr<Task> CSimpyEnv::create_task(F&& coroutine_func) {
    // Capture the callable into a heap-allocated std::function to ensure it lives
    auto func_holder = std::make_shared<std::decay_t<F>>(std::forward<F>(coroutine_func));

    // Invoke via the stored function to avoid dangling references to temporaries
    Task t = (*func_holder)();

    auto sp = std::make_shared<Task>(std::move(t));

    auto ce = std::make_shared<SimEvent>(*this);
    (sp->h).promise().set_completion_event(ce);

    active_tasks.push_back(sp);
    active_functors.push_back(func_holder);
    return sp;
}


struct ContainerBase {
    virtual bool can_put(int value) const = 0;
    virtual bool can_get(int value) const = 0;

    virtual ~ContainerBase() = default;
};

struct Container : ContainerBase {
    CSimpyEnv& env;
    int level = 0;
    int capacity;
    std::vector<std::pair<std::shared_ptr<SimEvent>, int>> get_waiters;
    std::vector<std::pair<std::shared_ptr<SimEvent>, int>> put_waiters;
    std::string name;

    Container(CSimpyEnv& e, int cap, std::string n = "") : env(e), capacity(cap), name(std::move(n)) {}

    auto put(int value);
    auto get(int value);

    bool can_put(int value) const override {
        return level + value <= capacity;
    }
    bool can_get(int value) const override {
        return level >= value;
    }

    void await_get(std::shared_ptr<SimEvent> get_event, int value) {
        get_waiters.emplace_back(std::move(get_event), value);
    }

    void await_put(std::shared_ptr<SimEvent> put_event, int value) {
        put_waiters.emplace_back(std::move(put_event), value);
    }

    // Set/get for label (existing methods)

    void trigger_get() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Get Waiters (before trying):\n";
            for (const auto& [h, v] : get_waiters) {
                std::cout << "[" << name << "]   - wants: " << v << "\n";
            }
        }
        for (size_t i = 0; i < get_waiters.size();) {
            auto& [get_event, v] = get_waiters[i];
            if (can_get(v)) {
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - try get : " << v << "\t"<<"level b4:"<<level<<"\n";
                }
                level -= v;
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - tre get : " << v << "\t"<<"level after:"<<level<<"\n";
                }
                get_event->on_succeed();

                get_waiters.erase(get_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void trigger_put() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Put Waiters (before trying):\n";
            for (const auto& [h, v] : put_waiters) {
                std::cout << "[" << name << "]   - wants to put: " << v << "\n";
            }
        }
        for (size_t i = 0; i < put_waiters.size();) {
            auto& [put_event, v] = put_waiters[i];
            if (can_put(v)) {
                level += v;
                put_event->on_succeed();
                put_waiters.erase(put_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    // Set/get for level
    void set_level(int l) { assert(l >= 0 && l <= capacity); level = l; }
    int get_level() const { return level; }
};


struct ContainerPutEvent : SimEvent {
    CSimpyEnv& env;
    Container& container;
    int value;
    ContainerPutEvent(CSimpyEnv& env_, Container& c, int v)
    : SimEvent(env_), env(env_), container(c), value(v) {
        sim_time = env.sim_time;
    }

    struct Awaiter {
        std::shared_ptr<ContainerPutEvent> self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto keep_alive = self; // make a copy of the shared_ptr
            // add put to queue
            self->container.await_put(keep_alive, self->value);

            // Separate callback to trigger gets first
            self->callbacks.emplace_back([keep_alive](int /*time*/) {
                keep_alive->container.trigger_get();
            });

            // Separate callback to resume coroutine
            self->callbacks.emplace_back([keep_alive, h](int time) {
                keep_alive->env.schedule(new CoroutineProcess(time, h, "ContainerPut::callback -> "));
            });
            self->container.trigger_put();
        }

        auto await_resume() { return self->value; }
    };

    auto operator co_await() && {
        auto ptr = std::make_shared<ContainerPutEvent>(std::move(*this));
        return Awaiter{ptr};
    }


    void trigger() {
        for (auto& cb : callbacks) {
            cb(env.sim_time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger();
    }

    virtual SimEvent* clone_for_schedule() const override {
        auto* clone = new ContainerPutEvent(env, container, value);
        clone->callbacks = callbacks;
        clone->done = done;

        return clone;
    }
};



struct ContainerGetEvent : SimEvent {
    CSimpyEnv& env;
    Container& container;
    int value;
    ContainerGetEvent(CSimpyEnv& env_, Container& c, int v)
    : SimEvent(env_), env(env_), container(c), value(v) {
        sim_time = env.sim_time;
    }

    struct Awaiter {
        std::shared_ptr<ContainerGetEvent> self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto keep_alive = self; // make a copy of the shared_ptr
            // add put to queue
            self->container.await_get(keep_alive, self->value);

            // Separate callback to trigger gets first
            self->callbacks.emplace_back([keep_alive](int /*time*/) {
                keep_alive->container.trigger_put();
            });

            // Separate callback to resume coroutine
            self->callbacks.emplace_back([keep_alive, h](int time) {
                keep_alive->env.schedule(new CoroutineProcess(time, h, "ContainerGet::callback -> "));
            });
            self->container.trigger_get();
        }

        auto await_resume() { return self->value; }
    };

    auto operator co_await() && {
        auto ptr = std::make_shared<ContainerGetEvent>(std::move(*this));
        return Awaiter{ptr};
    }


    void trigger() {
        for (auto& cb : callbacks) {
            cb(env.sim_time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger();
    }

    virtual SimEvent* clone_for_schedule() const override {
        auto* clone = new ContainerGetEvent(env, container, value);
        clone->callbacks = callbacks;
        clone->done = done;

        return clone;
    }
};

// Inline definitions for Container::put and Container::get
inline auto Container::put(int value) {
    return ContainerPutEvent(env, *this, value);
}

inline auto Container::get(int value) {
    return ContainerGetEvent(env, *this, value);
}




struct StorePutEvent;
struct StoreGetEvent;
// Store struct similar to Container but for ItemBase objects
struct Store {
    CSimpyEnv& env;
    size_t capacity;
    std::vector<std::shared_ptr<ItemBase>> items;
    std::vector<std::shared_ptr<StoreGetEvent>> get_waiters;
    std::vector<std::shared_ptr<StorePutEvent>> put_waiters;
    std::string name;

    Store(CSimpyEnv& e, size_t cap, std::string n = "")
        : env(e), capacity(cap), name(std::move(n)) {}

    bool can_put() const {
        return items.size() < capacity;
    }

    bool can_get() const {
        return !items.empty();
    }

    void await_put(std::shared_ptr<StorePutEvent> put_event);
    void await_get(std::shared_ptr<StoreGetEvent> get_event);
    void trigger_put();
    void trigger_get();
    void print_items() const;

    auto put(ItemBase& item, Priority priority = Priority::Low); // clone overload
    auto put(std::shared_ptr<ItemBase> item, Priority priority = Priority::Low); // take ownership overload
    auto get(std::function<bool(const std::shared_ptr<ItemBase>&)> filter = {}, Priority priority = Priority::Low);



};

// StorePutEvent
struct StorePutEvent : SimEvent {
    CSimpyEnv& env;
    Store& store;
    std::shared_ptr<ItemBase> item;
    Priority priority;

    StorePutEvent(CSimpyEnv& env_, Store& s, std::shared_ptr<ItemBase> it, Priority prio = Priority::Low)
        : SimEvent(env_), env(env_), store(s), item(std::move(it)), priority(prio) {
        sim_time = env.sim_time;

    }

    struct Awaiter {
        std::shared_ptr<StorePutEvent> self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto keep_alive = self;
            self->store.await_put(keep_alive);

            self->callbacks.emplace_back([keep_alive](int) {
                keep_alive->store.trigger_get();
            });

            self->callbacks.emplace_back([keep_alive, h](int t) {
                keep_alive->env.schedule(
                    new CoroutineProcess(t, h, "StorePut::callback -> "));
            });
            self->store.trigger_put();
        }

        auto await_resume() { return self->item; }
    };

    auto operator co_await() && {
        auto ptr = std::make_shared<StorePutEvent>(std::move(*this));
        return Awaiter{ptr};
    }

    void resume() override { trigger(); }

    SimEvent* clone_for_schedule() const override {
        auto* clone = new StorePutEvent(env, store, item, priority);
        clone->callbacks = callbacks;
        clone->done = done;
        return clone;
    }
};

// StoreGetEvent
struct StoreGetEvent : SimEvent {
    CSimpyEnv& env;
    Store& store;
    Priority priority;

    StoreGetEvent(CSimpyEnv& env_, Store& s,
                  std::function<bool(const std::shared_ptr<ItemBase>&)> filter = {}, Priority prio = Priority::Low)
        : SimEvent(env_), env(env_), store(s), priority(prio) {
        sim_time = env.sim_time;
        item_filter = std::move(filter);
    }

    struct Awaiter {
        std::shared_ptr<StoreGetEvent> self;

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto keep_alive = self;
            // Pass along the filter if present.
            self->store.await_get(keep_alive);

            self->callbacks.emplace_back([keep_alive](int) {
                keep_alive->store.trigger_put();
            });

            self->callbacks.emplace_back([keep_alive, h](int t) {
                keep_alive->env.schedule(
                    new CoroutineProcess(t, h, "StoreGet::callback -> "));
            });
            self->store.trigger_get();

        }

        auto await_resume() { return self->value; }
    };

    auto operator co_await() && {
        auto ptr = std::make_shared<StoreGetEvent>(std::move(*this));
        return Awaiter{ptr};
    }

    void resume() override { trigger(); }

    SimEvent* clone_for_schedule() const override {
        auto* clone = new StoreGetEvent(env, store, item_filter, priority);
        clone->callbacks = callbacks;
        clone->done = done;
        return clone;
    }
};

// Inline definition for Store::put
inline auto Store::put(ItemBase& item, Priority priority) {
    auto ptr = std::shared_ptr<ItemBase>(item.clone());
    return StorePutEvent(env, *this, std::move(ptr), priority);
}

// new overload: take ownership directly, no clone
inline auto Store::put(std::shared_ptr<ItemBase> item, Priority priority) {
    return StorePutEvent(env, *this, std::move(item), priority);
}

// Inline definition for Store::get
inline auto Store::get(std::function<bool(const std::shared_ptr<ItemBase>&)> filter, Priority priority) {
    return StoreGetEvent(env, *this, std::move(filter), priority);
}


// Inline definitions for Store methods
inline void Store::await_put(std::shared_ptr<StorePutEvent> put_event) {
    put_waiters.emplace_back(std::move(put_event));
}

inline void Store::await_get(std::shared_ptr<StoreGetEvent> get_event) {
    get_waiters.emplace_back(std::move(get_event));
}

inline void Store::trigger_put() {
    std::sort(put_waiters.begin(), put_waiters.end(),
        [](const std::shared_ptr<StorePutEvent>& a, const std::shared_ptr<StorePutEvent>& b) {
            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
        });
    for (size_t i = 0; i < put_waiters.size();) {
        auto& evt = put_waiters[i];
        if (can_put()) {
            items.push_back(evt->item);
            evt->on_succeed();
            put_waiters.erase(put_waiters.begin() + i);
        } else {
            ++i;
        }
    }
}

inline void Store::trigger_get() {
    std::sort(get_waiters.begin(), get_waiters.end(),
        [](const std::shared_ptr<StoreGetEvent>& a, const std::shared_ptr<StoreGetEvent>& b) {
            return static_cast<int>(a->priority) > static_cast<int>(b->priority);
        });
    for (size_t i = 0; i < get_waiters.size();) {
        auto& evt = get_waiters[i];
        auto it = std::find_if(items.begin(), items.end(),
                               [&evt](const std::shared_ptr<ItemBase>& item) {
                                   return !evt->item_filter || evt->item_filter(item);
                               });
        if (it != items.end()) {
            auto item = *it;
            items.erase(it);
            evt->set_value(item);
            evt->on_succeed();
            get_waiters.erase(get_waiters.begin() + i);
            continue;
        }
        ++i;
    }
}

inline void Store::print_items() const {
    std::cout << "[Store " << name << "] items:"<<items.size()<<std::endl;
    for (const auto& item : items) {
        std::cout << ' ' << item->to_string()<< std::endl;
    }
    std::cout << '\n';
}