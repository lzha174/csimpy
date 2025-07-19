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




struct SimEvent : SimEventBase {
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    std::variant<std::monostate, int, std::string> value;

    bool await_ready() const noexcept { return false; }

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
    SimEvent& ev;
    std::string label;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { ev.await_suspend(h, label); }
    auto await_resume() { return ev.await_resume(); }
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

Task processB(Task& td) {
    std::cout << "[" << sim_time << "] processB waiting...\n";
    auto sub_val = co_await LabeledAwait{td.get_completion_event(), "processB"};

    std::cout << "[" << sim_time << "] processB done waiting on sub_process\n";
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

    auto ts = subc();
    auto ta = processA(ts);
    auto tb = processB(ts);
    auto tt = trigger_process(shared_event);

    // Manually enqueue initial coroutine entries
    event_queue.push(new CoroutineProcess(0, ta.h, "processA"));
    event_queue.push(new CoroutineProcess(0, tb.h, "processB"));
    event_queue.push(new CoroutineProcess(0, ts.h, "sub_process"));
    event_queue.push(new CoroutineProcess(0, tt.h, "trigger_process"));

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