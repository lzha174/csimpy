#include "csimpy_env.h"
#include <iostream>

void CSimpyEnv::schedule(SimEventBase* e) {
    event_queue.push(e);
}

void CSimpyEnv::schedule(Task& t, const std::string& label) {
    auto* proc = new CoroutineProcess(0, t.h, label);
    schedule(proc);
}

void CSimpyEnv::run() {
    while (!event_queue.empty()) {
        //print_event_queue_state();  // 🔍 Print before processing

        SimEventBase* ev = event_queue.top();
        event_queue.pop();

        sim_time = ev->sim_time;
        ev->resume();  // resume the coroutine, which may enqueue again
        delete ev;
    }
}

Task CSimpyEnv::create_task(std::function<Task()> coroutine_func) {
    current_env = this;
    Task t = coroutine_func();  // Start coroutine

    return t;
}


Task TaskPromise::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}
