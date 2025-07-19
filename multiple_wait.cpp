// Awaitable for simulating a timed delay
#include <iostream>
#include <coroutine>
#include <queue>
#include <variant>
#include <string>
#include <type_traits>

int sim_time = 0;


// Scheduled coroutine event
struct ScheduledEvent {
    int sim_time;
    std::coroutine_handle<> handle;

    bool operator>(const ScheduledEvent& other) const {
        return sim_time > other.sim_time;
    }
};


std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<>> event_queue;

struct SimDelay {
    int wake_time;
    SimDelay(int delay) : wake_time(sim_time + delay) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        event_queue.push({wake_time, h});
    }

    void await_resume() const noexcept {}
};




struct SimEvent {
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
            event_queue.push({time, h});
        }
        waiters.clear();
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
    event_queue.push({0, tc.h});
    event_queue.push({0, ta.h});
    event_queue.push({0, tb.h});


    while (!event_queue.empty()) {
        auto ev = event_queue.top();
        event_queue.pop();

        sim_time = ev.sim_time;
        ev.handle.resume();  // resume the coroutine, which may enqueue again
    }

    return 0;
}