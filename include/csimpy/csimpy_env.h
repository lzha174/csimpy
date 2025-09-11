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
    std::shared_ptr<ItemBase> value;
    bool done = false;
    size_t unique_id;
    static inline std::atomic<size_t> uid_gen{0};
    static inline std::atomic<size_t> alloc_counter{0};
    static inline std::unordered_map<void*, size_t> alloc_map;
    SimEventBase() : unique_id(++uid_gen) {}
    virtual void resume() = 0;
    virtual ~SimEventBase() = default;
};


struct CompareSimEvent {
    bool operator()(const std::shared_ptr<SimEventBase>& a, const std::shared_ptr<SimEventBase>& b) const {
        if (a->sim_time != b->sim_time)
            return a->sim_time > b->sim_time;
        return a->unique_id > b->unique_id;
    }
};
class CSimpyEnv {
public:
    int sim_time = 0;

    std::priority_queue<
        std::shared_ptr<SimEventBase>,
        std::vector<std::shared_ptr<SimEventBase>>,
        CompareSimEvent
    > event_queue;
    std::vector<std::shared_ptr<Task>> active_tasks;
    std::vector<std::shared_ptr<void>> active_functors;
    // Track scheduled events by unique_id
    std::unordered_map<size_t, std::weak_ptr<SimEventBase>> scheduled_events;
    void schedule(std::shared_ptr<SimEventBase>);
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
        if (handle && !handle.done()) {   // ✅ only resume if still alive
            handle.resume();
        }
    }
};

// InterruptException definition
struct InterruptException : public std::exception {
    std::shared_ptr<ItemBase> cause;
    explicit InterruptException(std::shared_ptr<ItemBase> c) : cause(std::move(c)) {}
    const char* what() const noexcept override {
        return "Process interrupted";
    }
};

struct SimEvent : SimEventBase {

    CSimpyEnv& env;
    std::vector<std::function<void(int)>> callbacks;

    // Optional filter for Store get events
    std::function<bool(const std::shared_ptr<ItemBase>&)> item_filter;

    // Interrupt-related members
    bool interrupted = false;
    std::shared_ptr<ItemBase> interrupt_cause;

    std::string debug_label;

    SimEvent(CSimpyEnv& env_, std::string lbl = "") : env(env_), debug_label(std::move(lbl)) {
        sim_time = env.sim_time;
    }

    bool await_ready() const noexcept {
        if (!done) {
            return false;
        }
        return true;
    }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?");

    auto await_resume() const {
        if (interrupted) {
            throw InterruptException(interrupt_cause);
        }
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

    // Interrupt support
    virtual void interrupt(std::shared_ptr<ItemBase> cause = nullptr) {
        interrupted = true;
        interrupt_cause = std::move(cause);
        sim_time = env.sim_time;  // reschedule immediately
        on_succeed();
    }

    virtual std::shared_ptr<SimEvent> clone_for_schedule() const {
        auto clone = std::make_shared<SimEvent>(env);
        clone->value = value;
        clone->done = done;
        clone->interrupted = interrupted;
        clone->interrupt_cause = interrupt_cause;
        env.scheduled_events[clone->unique_id] = clone;
        return clone;
    }

    // Schedules a heap-allocated copy of this event at the given time and clears callbacks.
    virtual void on_succeed() {
        done = true;
        auto heap_event = this->clone_for_schedule();
        heap_event->callbacks = callbacks;
        //heap_event->sim_time = this->sim_time;
        env.schedule(heap_event);
    }
};




struct TaskPromise {
    std::shared_ptr<SimEvent> completion_event;
    SimEvent* current_event = nullptr;

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

    std::shared_ptr<SimEvent> get_completion_event() {
        return h.promise().completion_event;
    }

    CSimpyEnv& get_env() {
        return h.promise().completion_event->env;
    }

    bool done() const {
        return h.done();
    }

    void interrupt(std::shared_ptr<ItemBase> cause = nullptr) {
        auto& prom = h.promise();
        if (prom.current_event) {
            prom.current_event->interrupted = true;
            prom.current_event->interrupt_cause = cause;
            prom.current_event->interrupt();
        }
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

    SimDelay(CSimpyEnv& e, int d, std::string lbl = "")
        : SimEvent(e, std::move(lbl)) {
        delay = d;
        sim_time = env.sim_time + delay;
    }

    std::shared_ptr<SimEvent> clone_for_schedule() const override {
        auto clone = std::make_shared<SimDelay>(env, this->delay, this->debug_label);
        clone->done = done;
        env.scheduled_events[clone->unique_id] = clone;
        return clone;
    }

    void await_suspend(std::coroutine_handle<> h,
                       const std::string& label = "?") {
        callbacks.emplace_back([this, h, label](int when) {
            env.schedule(std::make_shared<CoroutineProcess>(
                when, h, "SimDelay::resume handler -> " + label));
        });
        auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
        ht.promise().current_event = this;
        this->on_succeed();
    }

    void resume() override {
        if constexpr (DEBUG_PRINT_QUEUE) {
            std::cout << "[" << env.sim_time << "] SimDelay resumed.\n";
        }
        trigger();
    }
    void interrupt(std::shared_ptr<ItemBase> cause = nullptr) override {
        interrupted = true;
        delay = 0;
        interrupt_cause = cause;
        sim_time = env.sim_time;  // override scheduled delay, fire now
        trigger();
    }
};
// Waits for all of a set of SimEvents to complete, then triggers itself.
struct AllOfEvent : SimEvent, std::enable_shared_from_this<AllOfEvent> {
    using SimEvent::SimEvent;  // inherit constructor
    std::vector<std::shared_ptr<SimEvent>> events;
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    int completed = 0;
    CSimpyEnv& env;

    AllOfEvent(CSimpyEnv& env_, std::vector<std::shared_ptr<SimEvent>> evts, std::string lbl = "")
        : SimEvent(env_, std::move(lbl)), events(std::move(evts)), env(env_) {
    }

    void count(int time) {
        assert(completed < static_cast<int>(events.size()));
        ++completed;
        if (completed == static_cast<int>(events.size())) {
            // Schedule this AllOfEvent itself (heap-allocated)
            auto self = shared_from_this();

            // Ensure this->value is always a valid FinishItem with populated map_value
            if (!this->value) {
                this->value = std::make_shared<MapItem>("allof", 101);
            }
            auto map_item = std::dynamic_pointer_cast<MapItem>(this->value);
            if (map_item) {
                for (const auto& ev : events) {
                    if (ev->value) {
                        map_item->map_value[ev->value->id] = ev->value;
                    }
                }
            }
            // assign the current time
            self->sim_time = env.sim_time;
            env.schedule(self);
        }
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        waiters.emplace_back(h, label);
        // Set the current event on the task promise
        auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
        ht.promise().current_event = this;
        auto self = shared_from_this();
        for (const std::shared_ptr<SimEvent>& e : events) {
            if (e->done && dynamic_cast<SimDelay*>(e.get()) == nullptr) {
                this->count(env.sim_time);
            } else if (auto* delay = dynamic_cast<SimDelay*>(e.get())) {
                e->callbacks.emplace_back([weak_self = std::weak_ptr<AllOfEvent>(self)](int t) {
                    if (auto s = weak_self.lock()) {
                            s->count(t);
                    }
                });
                delay->on_succeed();
            } else {
                e->callbacks.emplace_back([weak_self = std::weak_ptr<AllOfEvent>(self)](int t) {
                    if (auto s = weak_self.lock()) {

                            s->count(t);
                    }
                });
            }
        }
    }

    auto await_resume() const {
        if (interrupted) {
            throw InterruptException(interrupt_cause);
        }
        return value;
    }

    void resume() override {
        for (const auto& [wh, label] : waiters) {
            env.schedule(std::make_shared<CoroutineProcess>(env.sim_time, wh, "AllOfEvent::resume handler-> " + label));
        }
        waiters.clear();
    }

    virtual void on_succeed() override {
        if (done) return;
        done = true;
        this->sim_time = env.sim_time;
        env.schedule(shared_from_this());
    }

    // Override interrupt to mark as interrupted and remove callbacks from children
    void interrupt(std::shared_ptr<ItemBase> cause = nullptr) override {
        if (done) return;
        interrupted = true;
        interrupt_cause = std::move(cause);
        done = true;
        sim_time = env.sim_time;
        env.schedule(shared_from_this());
    }
};


// Waits for *any* of a set of SimEvents to complete, then triggers itself.
struct AnyOfEvent : SimEvent, std::enable_shared_from_this<AnyOfEvent> {
    using SimEvent::SimEvent;  // inherit constructor
    std::vector<std::shared_ptr<SimEvent>> events;
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    bool triggered = false;
    CSimpyEnv& env;

    AnyOfEvent(CSimpyEnv& env_, std::vector<std::shared_ptr<SimEvent>> evts)
        : SimEvent(env_), events(std::move(evts)), env(env_) {}

    void trigger_now(int time) {
        if (triggered) return; // prevent double trigger
        triggered = true;

        // Remove all callbacks from other events so they won't fire later
        for (auto& e : events) {
            e->callbacks.clear();
        }

        // Schedule this AnyOfEvent
        auto self = shared_from_this();
        self->sim_time = time;
        env.schedule(self);
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        waiters.emplace_back(h, label);
        // Set the current event on the task promise
        auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
        ht.promise().current_event = this;
        auto self = shared_from_this();
        for (auto& e : events) {
            e->callbacks.emplace_back([weak_self = std::weak_ptr<AnyOfEvent>(self)](int t) {
                if (auto s = weak_self.lock()) {

                        s->trigger_now(t);
                }
            });

            // Handle events that are already done
            if (e->done && dynamic_cast<SimDelay*>(e.get()) == nullptr) {
                // it still has callback to anyof event and is not a SimDelay
                e->on_succeed();
            }

            if (auto* delay = dynamic_cast<SimDelay*>(e.get())) {
                delay->on_succeed();
            }
        }
    }

    auto await_resume() const {
        if (interrupted) {
            throw InterruptException(interrupt_cause);
        }
        return std::string("any_done");
    }

    void resume() override {
        for (const auto& [wh, label] : waiters) {
            env.schedule(std::make_shared<CoroutineProcess>(env.sim_time, wh, "AnyOfEvent::resume handler-> " + label));
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


struct ContainerPutEvent : SimEvent, std::enable_shared_from_this<ContainerPutEvent> {
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
            // Separate callback to resume coroutine
            self->callbacks.emplace_back([keep_alive, h](int time) {
                keep_alive->env.schedule(std::make_shared<CoroutineProcess>(time, h, "ContainerPut::callback -> "));
            });
            auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
            ht.promise().current_event = self.get();
        }

        auto await_resume() { return self->value; }
    };



    void trigger() {
        for (auto& cb : callbacks) {
            cb(env.sim_time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger();
    }

    void on_succeed() override {
        assert(dynamic_cast<ContainerPutEvent*>(this) == this);
        done = true;
        this->sim_time = env.sim_time;
        env.schedule(shared_from_this());
    }

};



struct ContainerGetEvent : SimEvent, std::enable_shared_from_this<ContainerGetEvent> {
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

            // Separate callback to resume coroutine
            self->callbacks.emplace_back([keep_alive, h](int time) {
                keep_alive->env.schedule(std::make_shared<CoroutineProcess>(time, h, "ContainerGet::callback -> "));
            });
            auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
            ht.promise().current_event = self.get();
        }

        auto await_resume() { return self->value; }
    };

    void trigger() {
        for (auto& cb : callbacks) {
            cb(env.sim_time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger();
    }
    void on_succeed() override {
        assert(dynamic_cast<ContainerGetEvent*>(this) == this);
        done = true;
        // Record the simulation time in the event itself
        this->sim_time = env.sim_time;

        env.schedule(shared_from_this());
    }
};

// Inline definitions for Container::put and Container::get
inline auto Container::put(int value) {
    auto put_event_ptr = std::make_shared<ContainerPutEvent>(env, *this, value);
    await_put(put_event_ptr, value);
    // Trigger the opposite side first so any waiting getters can proceed.
    put_event_ptr->callbacks.emplace_back([this](int) {
        this->trigger_get();
    });
    // Try to fulfill puts immediately if possible.
    trigger_put();
    return put_event_ptr;
}

inline auto Container::get(int value) {
    auto get_event_ptr = std::make_shared<ContainerGetEvent>(env, *this, value);
    await_get(get_event_ptr, value);
    // Trigger the opposite side first so any waiting putters can proceed.
    get_event_ptr->callbacks.emplace_back([this](int) {
        this->trigger_put();
    });
    // Try to fulfill gets immediately if possible.
    trigger_get();
    return get_event_ptr;
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
    auto get(std::shared_ptr<std::function<bool(const std::shared_ptr<ItemBase>&)>> filter_ptr, Priority priority = Priority::Low);
private:
    auto _put_impl(std::shared_ptr<ItemBase> item, Priority priority);


};

// StorePutEvent
struct StorePutEvent : SimEvent, std::enable_shared_from_this<StorePutEvent> {
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
            self->callbacks.emplace_back([keep_alive, h](int t) {
                keep_alive->env.schedule(
                    std::make_shared<CoroutineProcess>(t, h, "StorePut::callback -> "));
            });
            auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
            ht.promise().current_event = self.get();
        }

        auto await_resume() { return self->item; }
    };

    void resume() override { trigger(); }

    // Remove clone_for_schedule

    void on_succeed() override {
        assert(dynamic_cast<StorePutEvent*>(this) == this);
        done = true;
        this->sim_time = env.sim_time;
        env.schedule(shared_from_this());
    }
};

// StoreGetEvent
struct StoreGetEvent : SimEvent, std::enable_shared_from_this<StoreGetEvent> {
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

        // Rewritten to avoid re-wrapping shared_ptr
        void await_suspend(std::coroutine_handle<> h) {
            auto keep_alive = self; // already a shared_ptr
            self->callbacks.emplace_back([keep_alive, h](int t) {
                keep_alive->env.schedule(
                    std::make_shared<CoroutineProcess>(t, h, "StoreGet::callback -> "));
            });
            auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
            ht.promise().current_event = self.get();
        }

        auto await_resume() { return self->value; }
    };

    void resume() override { trigger(); }

    // Remove clone_for_schedule

    void on_succeed() override {
        assert(dynamic_cast<StoreGetEvent*>(this) == this);
        done = true;
        this->sim_time = env.sim_time;
        env.schedule(shared_from_this());
    }
};


inline StoreGetEvent::Awaiter operator co_await(std::shared_ptr<StoreGetEvent> event) {
    return StoreGetEvent::Awaiter{event};
}

inline StorePutEvent::Awaiter operator co_await(std::shared_ptr<StorePutEvent> event) {
    return StorePutEvent::Awaiter{event};
}

inline ContainerGetEvent::Awaiter operator co_await(std::shared_ptr<ContainerGetEvent> event) {
    return ContainerGetEvent::Awaiter{event};
}

inline ContainerPutEvent::Awaiter operator co_await(std::shared_ptr<ContainerPutEvent> event) {
    return ContainerPutEvent::Awaiter{event};
}

// Private helper for Store::put
inline auto Store::_put_impl(std::shared_ptr<ItemBase> item, Priority priority) {
    auto put_event_ptr = std::make_shared<StorePutEvent>(StorePutEvent(env, *this, std::move(item), priority));
    await_put(put_event_ptr);
    put_event_ptr->callbacks.emplace_back([this](int) {
        this->trigger_get();
    });
    trigger_put();
    return put_event_ptr;
}

inline auto Store::put(ItemBase& item, Priority priority) {
    return _put_impl(std::shared_ptr<ItemBase>(item.clone()), priority);
}

inline auto Store::put(std::shared_ptr<ItemBase> item, Priority priority) {
    return _put_impl(std::move(item), priority);
}


// Overload taking a shared_ptr filter to extend filter lifetime
inline auto Store::get(std::shared_ptr<std::function<bool(const std::shared_ptr<ItemBase>&)>> filter_ptr, Priority priority) {
    std::function<bool(const std::shared_ptr<ItemBase>&)> filter = filter_ptr ? *filter_ptr : std::function<bool(const std::shared_ptr<ItemBase>&)>();
    auto get_event_ptr = std::make_shared<StoreGetEvent>(env, *this, std::move(filter), priority);
    await_get(get_event_ptr);
    get_event_ptr->callbacks.emplace_back([this](int) {
        this->trigger_put();
    });
    trigger_get();
    return get_event_ptr;
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
    //std::cout << "waiter size "<<get_waiters.size()<<std::endl;
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

// Out-of-line definition for SimEvent::await_suspend
inline void SimEvent::await_suspend(std::coroutine_handle<> h, const std::string& label) {
    callbacks.emplace_back([this, h, label](int time) {
        env.schedule(std::make_shared<CoroutineProcess>(time, h, "SimEvent::callback -> " + label));
    });
    auto ht = std::coroutine_handle<TaskPromise>::from_address(h.address());
    ht.promise().current_event = this;
}