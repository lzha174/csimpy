#include <iostream>
#include <coroutine>
#include <queue>
#include <variant>
#include <string>

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

// Awaitable simulation event
struct SimEvent {
    int scheduled_time;
    std::coroutine_handle<> handle = nullptr;
    std::variant<std::monostate, int, std::string> value;

    SimEvent(int when) : scheduled_time(when) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        handle = h;
        event_queue.push({scheduled_time, h});
    }

    auto await_resume() const {
        return value;
    }

    template<typename T>
    void set_value(T val) {
        value = val;
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
Task process() {
    std::cout << "[" << sim_time << "] process() started\n";

    SimEvent ev1(5);
    ev1.set_value(std::string("hello"));

    auto result = co_await ev1;
    std::cout << "[" << sim_time << "] process() resumed with: " << std::get<std::string>(result) << "\n";
}

int main() {
    auto t = process();  // creates but suspends
    event_queue.push({0, t.h});  // manually enqueue initial start event at time 0

    while (!event_queue.empty()) {
        auto ev = event_queue.top();
        event_queue.pop();

        sim_time = ev.sim_time;
        ev.handle.resume();  // resume the coroutine, which may enqueue again
    }

    return 0;
}