#pragma once
#include <queue>
#include <coroutine>
#include <functional>
#include <string>

#include "csimpy_env.h"


struct CompareSimEvent;
struct CoroutineProcess;
class SimEvent;
class Task;

struct SimEventBase {
    int sim_time;
    virtual void resume() = 0;
    virtual ~SimEventBase() = default;
};


struct CompareSimEvent {
    bool operator()(const SimEventBase* a, const SimEventBase* b) {
        return a->sim_time > b->sim_time;
    }
};
class CSimpyEnv {
public:
    int sim_time = 0;

    std::priority_queue<SimEventBase*, std::vector<SimEventBase*>, CompareSimEvent> event_queue;

    void schedule(SimEventBase*);
    void schedule(Task&, const std::string&);
    Task create_task(std::function<Task()> coroutine_func);
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
    std::vector<std::pair<std::coroutine_handle<>, std::string>> waiters;
    std::variant<std::monostate, int, std::string> value;
    CSimpyEnv& env;

    SimEvent(CSimpyEnv& env_) : env(env_) {}

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
        for (const auto& [h, label] : waiters) {
            env.schedule(new CoroutineProcess(time, h, "SimEvent::trigger -> " + label));
        }
        waiters.clear();
    }

    void resume() override {
        trigger(env.sim_time);
    }
};





// Coroutine type

// Abstract base class for simulation events








inline thread_local CSimpyEnv* current_env = nullptr;

struct TaskPromise {
    CSimpyEnv& env;
    SimEvent completion_event;

    TaskPromise() : env(*current_env), completion_event(env) {
    }

    Task get_return_object();

    std::suspend_always initial_suspend() { return {}; }
    auto final_suspend() noexcept {
        struct FinalAwaiter {
            TaskPromise* promise;

            bool await_ready() noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) const noexcept {
                promise->completion_event.set_value(std::string("done"));
                promise->completion_event.trigger(promise->env.sim_time);
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
    ~Task() {
        if (h) h.destroy();
    }

    SimEvent& get_completion_event() {
        return h.promise().completion_event;
    }

    CSimpyEnv& get_env() {
        return h.promise().env;
    }

    bool done() const {
        return h.done();
    }
};




struct LabeledAwait {
    SimEvent& ev;
    std::string label;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { ev.await_suspend(h, label); }
    auto await_resume() { return ev.await_resume(); }
};


