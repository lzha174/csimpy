#pragma once
#include <queue>
#include <coroutine>
#include <functional>
#include <iostream>
#include <string>

#include "csimpy_env.h"


struct AllOfEvent;
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
    std::variant<std::monostate, int, std::string> value;
    CSimpyEnv& env;
    std::vector<std::function<void(int)>> callbacks;

    SimEvent(CSimpyEnv& env_) : env(env_) {}

    bool await_ready() const noexcept { return false; }

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
        for (auto& cb : callbacks) {
            cb(time);
        }
        callbacks.clear();
    }

    void resume() override {
        trigger(env.sim_time);
    }

    // Schedules a heap-allocated copy of this event at the given time and clears callbacks.
    void on_succeed(int time) {
        auto heap_event = new SimEvent(env);
        heap_event->value = this->value;
        heap_event->callbacks = std::move(this->callbacks);
        heap_event->sim_time = time;
        this->callbacks.clear();
        env.schedule(heap_event);
    }
};




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
    SimEvent& event;
    std::string label;

    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { event.await_suspend(h, label); }
    auto await_resume() { return event.await_resume(); }
};

struct SimDelay : SimEvent {
    SimDelay(CSimpyEnv& e, int delay)
        : SimEvent(e) {
        sim_time = e.sim_time + delay;
    }

    // schedule a heap-allocated copy, not `this`
    void await_suspend(std::coroutine_handle<> h,
                       const std::string& label = "?") {
        // store the coroutine-resumption callback
        callbacks.emplace_back([this, h, label](int when) {
            env.schedule(new CoroutineProcess(
                when, h, "SimDelay::resume handler -> " + label));
            //env.print_event_queue_state();
        });

        // ----- clone onto heap using smart pointer -----
        int remaining = sim_time - env.sim_time;          // delay left
        auto heapDelay = std::make_unique<SimDelay>(env, remaining);   // smart pointer
        heapDelay->callbacks = std::move(callbacks);      // move callbacks
        callbacks.clear();                                // (optional)

        env.schedule(heapDelay.release());   // release raw pointer to pass to schedule
    }

    void resume() override {
        std::cout << "[" << env.sim_time << "] SimDelay resumed.\n";
        trigger(sim_time);   // execute stored callbacks
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

            if (auto* delay = dynamic_cast<SimDelay*>(e)) {
                // Force scheduling of delay event on the heap if it hasn’t been already
                int remaining = delay->sim_time - env.sim_time;
                auto heapDelay = std::make_unique<SimDelay>(env, remaining);
                heapDelay->callbacks = std::move(delay->callbacks); // transfer callbacks
                delay->callbacks.clear();
                env.schedule(heapDelay.release());

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
