constexpr bool DEBUG_RESOURCE = false;
// Awaitable for simulating a timed delay
#include <iostream>
#include <coroutine>
#include <queue>
#include <variant>
#include <string>
#include <type_traits>

int sim_time = 0;


// Abstract base class for simulation events
struct SimEventBase {
    int sim_time;
    virtual void resume() = 0;
    virtual ~SimEventBase() = default;
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

struct CompareSimEvent {
    bool operator()(const SimEventBase* a, const SimEventBase* b) {
        return a->sim_time > b->sim_time;
    }
};

std::priority_queue<SimEventBase*, std::vector<SimEventBase*>, CompareSimEvent> event_queue;

void print_event_queue_state() {
    std::vector<SimEventBase*> temp;

    std::cout << "🪄 Event Queue @ time " << sim_time << ":\n";

    // Temporarily pop elements to inspect
    while (!event_queue.empty()) {
        auto e = event_queue.top();
        event_queue.pop();
        std::cout << "  - Scheduled at: " << e->sim_time;
        if (auto ce = dynamic_cast<CoroutineProcess*>(e)) {
            std::cout << " [Coroutine: " << ce->label << "]";
        } else {
            std::cout << " (" << typeid(*e).name() << ")";
        }
        std::cout << "\n";
        temp.push_back(e);
    }

    // Restore the queue
    for (auto* e : temp) {
        event_queue.push(e);
    }
}




struct SimDelay {
    int wake_time;
    SimDelay(int delay) : wake_time(sim_time + delay) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        event_queue.push(new CoroutineProcess(wake_time, h, "SimDelay"));
    }

    void await_resume() const noexcept {}
};


struct ContainerBase {
    virtual void put(int value) = 0;
    virtual bool can_get(int value) const = 0;
    virtual void get(int value) = 0;
    virtual ~ContainerBase() = default;
};


struct Container : ContainerBase {
    int level = 0;
    int capacity;
    std::vector<std::pair<std::coroutine_handle<>, int>> get_waiters;
    std::vector<std::pair<std::coroutine_handle<>, int>> put_waiters;
    std::string name;

    Container(int cap, std::string n = "") : capacity(cap), name(std::move(n)) {}

    bool can_put(int value) const {
        return level + value <= capacity;
    }
    bool can_get(int value) const override {
        return level >= value;
    }

    void await_get(std::coroutine_handle<> h, int value) {
        get_waiters.emplace_back(h, value);
    }

    void await_put(std::coroutine_handle<> h, int value) {
        put_waiters.emplace_back(h, value);
    }

    void try_wake_get_waiters() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Get Waiters (before trying):\n";
            for (const auto& [h, v] : get_waiters) {
                std::cout << "[" << name << "]   - wants: " << v << "\n";
            }
        }
        for (size_t i = 0; i < get_waiters.size();) {
            auto [h, v] = get_waiters[i];
            if (can_get(v)) {
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - try get : " << v << "\t"<<"level b4:"<<level<<"\n";
                }
                level -= v;
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - tre get : " << v << "\t"<<"level after:"<<level<<"\n";
                }
                event_queue.push(new CoroutineProcess(sim_time, h, "Container::resume get"));

                get_waiters.erase(get_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void try_wake_put_waiters() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Put Waiters (before trying):\n";
            for (const auto& [h, v] : put_waiters) {
                std::cout << "[" << name << "]   - wants to put: " << v << "\n";
            }
        }
        for (size_t i = 0; i < put_waiters.size();) {
            auto [h, v] = put_waiters[i];
            if (can_put(v)) {
                level += v;
                event_queue.push(new CoroutineProcess(sim_time, h, "Container::resume put"));
                put_waiters.erase(put_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void put(int value) override {
        if (can_put(value)) {
            if (DEBUG_RESOURCE) {
                std::cout << "[" << name << "]   - put : " << value << "\t"<<"level before :" << level << "\n";
            }
            level += value;
            if (DEBUG_RESOURCE) {
                std::cout << "[" << name << "]   - put : " << value << "\t"<<"level  after:" << level << "\n";
            }
            try_wake_get_waiters();  // notify get waiters
        } else {
            // Queue if over capacity, assume caller will suspend
        }
    }

    void get(int value) override {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "]   - get : " << value << "\t"<<"level :" << level << "\n";
        }
        level -= value;
        try_wake_put_waiters();  // notify put waiters
    }
};



struct ContainerPutEvent {
    Container& container;
    int value;

    ContainerPutEvent(Container& c, int v) : container(c), value(v) {}

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        container.await_put(h, value);
        container.try_wake_put_waiters();
    }

    void await_resume() const noexcept {
        container.try_wake_get_waiters();
    }
};

struct ContainerGetEvent {
    Container& container;
    int value;

    ContainerGetEvent(Container& c, int v) : container(c), value(v) {}

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        container.await_get(h, value);
        container.try_wake_get_waiters();  // try to fulfill get immediately
    }

    void await_resume() const noexcept {
        container.try_wake_put_waiters();
    }
};



struct SimEvent : SimEventBase {
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    std::variant<std::monostate, int, std::string> value;
    bool done = false;
    bool await_ready() const noexcept {
        if (!done) {
            return false;
        }
        return true;
    }

    void await_suspend(std::coroutine_handle<> h, const std::string& label = "?") {
        waiters.emplace_back(h, label);  // allow multiple waiters with labels
    }

    auto await_resume() const {
        return value;
    }

    template<typename T>
    void set_value(T val) {
        static_assert(
            std::disjunction_v<
                std::is_same<T, int>,
                std::is_same<T, std::string>
            >,
            "SimEvent::set_value() only accepts int or std::string"
        );

        value = val;
    }

    void trigger(int time) {
        for (size_t i = 0; i < waiters.size(); ++i) {
            const auto& [h, label] = waiters[i];
            event_queue.push(new CoroutineProcess(time, h, "SimEvent::trigger -> " + label));
        }
        waiters.clear();
        done=true;
    }

    void resume() override {
        trigger(sim_time);
    }
};


// Coroutine type
struct Task;

struct TaskPromise {
    SimEvent completion_event;

    Task get_return_object();

    std::suspend_always initial_suspend() { return {}; }
    auto final_suspend() noexcept {
        struct FinalAwaiter {
            TaskPromise* promise;

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) const noexcept {
                promise->completion_event.set_value(std::string("done"));
                promise->completion_event.trigger(sim_time);
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

    SimEvent& get_completion_event() {
        return h.promise().completion_event;
    }
};

Task TaskPromise::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}

struct LabeledAwait {
    SimEvent& event;
    std::string label;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { event.await_suspend(h, label); }
    auto await_resume() { return event.await_resume(); }
};

Task subc() {
    std::cout << "[" << sim_time << "] sub_process started\n";
    co_await SimDelay(130);
    std::cout << "[" << sim_time << "] sub_process finished\n";
}

Task processA(Task& td) {
    std::cout << "[" << sim_time << "] processA waiting...\n";
    auto sub_val = co_await LabeledAwait{td.get_completion_event(), "processA"};

    std::cout << "[" << sim_time << "] processA done waiting on sub_process\n";
}

Task processB(Task& td, Task& tf) {
    std::cout << "[" << sim_time << "] processB waiting...\n";
    auto sub_val = co_await LabeledAwait{td.get_completion_event(), "processB"};

    std::cout << "[" << sim_time << "] processB done waiting on sub_process\n";
    co_await tf.get_completion_event();
    std::cout << "[" << sim_time << "] processB done waiting on task\n";
}

Task trigger_process(SimEvent& e) {
    std::cout << "[" << sim_time << "] trigger_process waiting 5...\n";
    co_await SimDelay(5);
    std::cout << "[" << sim_time << "] trigger_process scheduling trigger...\n";
    e.trigger(sim_time);
    e.set_value(std::string("done"));
    co_return;
}

Container test_container(15, "test_container");


Task test_put_first() {
    co_await SimDelay(5);  // wait 5 units
    std::cout << "[" << sim_time << "] test_put_first: putting 4\n";
    co_await ContainerPutEvent(test_container, 4);
    std::cout << "[" << sim_time << "] test_put_first: done\n";

    co_await SimDelay(5);  // wait 5 units
    std::cout << "[" << sim_time << "] test_put_first: putting 10\n";
    co_await ContainerPutEvent(test_container, 10);
    std::cout << "[" << sim_time << "] test_put_first: done\n";
}

Task test_get_second() {
    co_await SimDelay(6);  // wait 5 units
    std::cout << "[" << sim_time << "] test_get_second: trying to get 3"<< " current level  "<<test_container.level << std::endl;;
    co_await ContainerGetEvent(test_container, 3);
    std::cout << "[" << sim_time << "] test_get_second: got 3"<< " current level  "<<test_container.level << std::endl;;

    std::cout << "[" << sim_time << "] test_get_second: trying to get 9"<< " current level  "<<test_container.level << std::endl;
    co_await ContainerGetEvent(test_container, 9);
    std::cout << "[" << sim_time << "] test_get_second: got 9" << " current level  "<<test_container.level << std::endl;
}



int main() {
    SimEvent shared_event;

    auto ts = subc();
    auto ta = processA(ts);
    auto tb = processB(ts, ta);
    auto tt = trigger_process(shared_event);

    auto tput = test_put_first();
    auto tget = test_get_second();

    // Manually enqueue initial coroutine entries
     event_queue.push(new CoroutineProcess(0, ta.h, "processA"));
     event_queue.push(new CoroutineProcess(0, tb.h, "processB"));
     event_queue.push(new CoroutineProcess(0, ts.h, "sub_process"));
    //event_queue.push(new CoroutineProcess(0, tt.h, "trigger_process"));
    //event_queue.push(new CoroutineProcess(0, tput.h, "test_put_first"));
    //event_queue.push(new CoroutineProcess(0, tget.h, "test_get_second"));

    while (!event_queue.empty()) {
        //print_event_queue_state();  // 🔍 Print before processing

        SimEventBase* ev = event_queue.top();
        event_queue.pop();

        sim_time = ev->sim_time;
        ev->resume();  // resume the coroutine, which may enqueue again
        delete ev;
    }



    return 0;
}