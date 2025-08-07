#include "../../include/csimpy/csimpy_env.h"
#include <iostream>

void CSimpyEnv::schedule(std::shared_ptr<SimEventBase> ev) {
    event_queue.push(std::move(ev));
}

void CSimpyEnv::schedule(std::shared_ptr<Task> t, const std::string& label) {
    auto proc = std::make_shared<CoroutineProcess>(this->sim_time, t->h, label);
    schedule(proc);
}

void CSimpyEnv::run() {
    while (!event_queue.empty()) {
        print_event_queue_state();  // 🔍 Print before processing

        std::shared_ptr<SimEventBase> ev = event_queue.top();
        event_queue.pop();

        sim_time = ev->sim_time;
        ev->resume();  // resume the coroutine, which may enqueue again

        // No need to manually delete ev, shared_ptr manages lifetime
    }
}



void CSimpyEnv::print_event_queue_state() {
    if (!DEBUG_PRINT_QUEUE) return;

    std::cout << "🪄 Event Queue @ time " << sim_time << ":\n";

    std::vector<std::shared_ptr<SimEventBase>> temp;

    // Temporarily pop elements to inspect
    while (!event_queue.empty()) {
        std::shared_ptr<SimEventBase> e = event_queue.top();
        event_queue.pop();
        std::cout << "  - Scheduled at: " << e->sim_time;
        if (auto ce = std::dynamic_pointer_cast<CoroutineProcess>(e)) {
            std::cout << " [Coroutine: " << ce->label << "]";
        } else {
            std::cout << " (" << typeid(*e.get()).name() << ")";
        }
        std::cout << "\n";
        temp.push_back(e);
    }

    // Restore the queue
    for (const auto& e : temp) {
        event_queue.push(e);
    }
}


Task TaskPromise::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}
