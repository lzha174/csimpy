#include <iostream>
#include <coroutine>
#include <queue>
#include <variant>
#include <string>

int sim_time = 0;

struct ScheduledEvent {
    int sim_time;
    std::coroutine_handle<> handle;

    bool operator>(const ScheduledEvent& other) const {
        return sim_time > other.sim_time;
    }
};

std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<>> event_queue;

// --- Awaitable SimEvent ---
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

// --- Coroutine that waits on SimEvent ---
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::exit(1); }
    };
};

// --- Generator-like coroutine ---
Task process() {
    std::cout << "[" << sim_time << "] Start process\n";

    SimEvent ev1(5);  // schedule to resume at time = 5
    ev1.set_value(std::string("hello"));

    auto result = co_await ev1;
    std::cout << "[" << sim_time << "] Resumed with: " << std::get<std::string>(result) << "\n";
}

int main() {
    process();  // start coroutine

    while (!event_queue.empty()) {
        auto ev = event_queue.top();
        event_queue.pop();

        sim_time = ev.sim_time;
        ev.handle.resume();  // this may push more events
    }

    return 0;
}