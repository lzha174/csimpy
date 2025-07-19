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
struct CoroutineEvent : SimEventBase {
    std::coroutine_handle<> handle;
    CoroutineEvent(int t, std::coroutine_handle<> h) {
        sim_time = t;
        handle = h;
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

struct SimDelay {
    int wake_time;
    SimDelay(int delay) : wake_time(sim_time + delay) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        event_queue.push(new CoroutineEvent(wake_time, h));
    }

    void await_resume() const noexcept {}
};




struct SimEvent : SimEventBase {
    std::vector<std::coroutine_handle<>> waiters;
    std::variant<std::monostate, int, std::string> value;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        waiters.push_back(h);  // allow multiple waiters
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
        for (auto h : waiters) {
            event_queue.push(new CoroutineEvent(time, h));
        }
        waiters.clear();
    }

    void resume() override {
        trigger(sim_time);
    }
};

// Coroutine type
struct Task;

struct TaskPromise {
    Task get_return_object();

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_void() {}
    void unhandled_exception() { std::exit(1); }
};

struct Task {
    using promise_type = TaskPromise;
    std::coroutine_handle<TaskPromise> h;

    explicit Task(std::coroutine_handle<TaskPromise> h) : h(h) {}
};

Task TaskPromise::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}

// The coroutine process that we’ll start via event queue
Task processA(SimEvent& e) {
    std::cout << "[" << sim_time << "] processA waiting...\n";
    auto val = co_await e;
    std::cout << "[" << sim_time << "] processA resumed with: " << std::get<std::string>(val) << "\n";
}

Task processB(SimEvent& e) {
    std::cout << "[" << sim_time << "] processB waiting...\n";
    auto val = co_await e;
    std::cout << "[" << sim_time << "] processB resumed with: " << std::get<std::string>(val) << "\n";
}

Task trigger_process(SimEvent& e) {
    std::cout << "[" << sim_time << "] trigger_process waiting 5...\n";
    co_await SimDelay(5);
    std::cout << "[" << sim_time << "] trigger_process scheduling trigger...\n";
    e.trigger(sim_time);
    e.set_value(std::string("done"));
    co_return;
}

int main() {
    SimEvent shared_event;

    auto ta = processA(shared_event);
    auto tb = processB(shared_event);
    auto tc = trigger_process(shared_event);

    // Manually enqueue initial coroutine entries
    event_queue.push(new CoroutineEvent(0, tc.h));
    event_queue.push(new CoroutineEvent(0, ta.h));
    event_queue.push(new CoroutineEvent(0, tb.h));

    while (!event_queue.empty()) {
        SimEventBase* ev = event_queue.top();
        event_queue.pop();

        sim_time = ev->sim_time;
        ev->resume();  // resume the coroutine, which may enqueue again
        delete ev;
    }

    return 0;
}