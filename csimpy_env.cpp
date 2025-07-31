#include "csimpy_env.h"
#include <iostream>

void CSimpyEnv::schedule(SimEventBase* e) {
    event_queue.push(e);
}

void CSimpyEnv::schedule(std::shared_ptr<Task> t, const std::string& label) {
    auto* proc = new CoroutineProcess(this->sim_time, t->h, label);
    schedule(proc);
}

void CSimpyEnv::run() {
    while (!event_queue.empty()) {
        print_event_queue_state();  // 🔍 Print before processing

        SimEventBase* ev = event_queue.top();
        event_queue.pop();

        sim_time = ev->sim_time;
        ev->resume();  // resume the coroutine, which may enqueue again

        delete ev; // normal event can be deleted immediately

    }
    if (!SimEventBase::alloc_map.empty()) {
        std::cerr << "⚠️ Leaked events: " << SimEventBase::alloc_map.size() << "\n";
    }
}



void CSimpyEnv::print_event_queue_state() {
    if (!DEBUG_PRINT_QUEUE) return;

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


Task TaskPromise::get_return_object() {
    return Task{std::coroutine_handle<TaskPromise>::from_promise(*this)};
}
