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
constexpr bool DEBUG_PRINT_QUEUE = true;
struct SimEventBase {
    int sim_time;
    int delay = 0;
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
    std::vector<std::shared_ptr<Task>> active_tasks;
    void schedule(SimEventBase*);
    void schedule(std::shared_ptr<Task> t, const std::string& label) ;
    std::shared_ptr<Task> create_task(std::function<Task()> coroutine_func);
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
    bool done = false;

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
        done=true; // finished all callbacks..done is true..
    }

    void resume() override {
        trigger(env.sim_time);
    }

    virtual SimEvent* clone_for_schedule() const {
        auto* clone = new SimEvent(env);
        clone->value = value;

        return clone;
    }

    // Schedules a heap-allocated copy of this event at the given time and clears callbacks.
    void on_succeed() {
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
                    promise->completion_event->set_value(std::string("done"));
                    promise->completion_event->trigger(
                        promise->completion_event->env.sim_time);
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
        return new SimDelay(env, delay);
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
        if (DEBUG_PRINT_QUEUE) std::cout << "[" << env.sim_time << "] SimDelay resumed.\n";
        trigger(env.sim_time);
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
