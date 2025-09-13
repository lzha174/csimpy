#include "include//csimpy//csimpy_env.h"


#include "include//csimpy//csimpy_env.h" // change accordingly

void example_porcess_interrupt() {
    CSimpyEnv env;
    auto worker = env.create_task([&env]() -> Task {
    try {
        std::cout << "[" << env.sim_time << "] worker: starting long delay\n";
        co_await SimDelay(env, 20, "long_delay");
        std::cout << "[" << env.sim_time << "] worker: finished long delay (not interrupted)\n";
    } catch (const InterruptException& ex) {
        std::cout << "[" << env.sim_time << "] worker: interrupted! Cause: "
                  << (ex.cause ? ex.cause->to_string() : "(none)") << std::endl;
    }
});

    auto controller = env.create_task([&env, &worker]() -> Task {
        co_await SimDelay(env, 5, "controller_delay");
        std::cout << "[" << env.sim_time << "] controller: interrupting worker\n";
        worker->interrupt(std::make_shared<SimpleItem>("urgent_call", 999));
        std::cout << "[" << env.sim_time << "] controller: worker interrupted\n";
    });

    env.schedule(worker, "worker");
    env.schedule(controller, "controller");
    env.run();
}


void patient_flow() {
    CSimpyEnv env;

    auto register_task = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] patient starts registration\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] patient finishes registration\n";
    });

    auto reg_event = register_task->get_completion_event(); // capture once

    auto see_doctor_task = env.create_task([&env, &reg_event]() -> Task {
        co_await *reg_event;
        std::cout << "[" << env.sim_time << "] patient starts seeing doctor\n";
        co_await SimDelay(env, 20);
        std::cout << "[" << env.sim_time << "] patient finishes seeing doctor\n";
    });

    auto lab_test_task = env.create_task([&env, &reg_event]() -> Task {
        co_await *reg_event;
        std::cout << "[" << env.sim_time << "] patient starts lab test\n";
        co_await SimDelay(env, 40);
        std::cout << "[" << env.sim_time << "] patient finishes lab test\n";
    });

    auto signout_task = env.create_task([&env, &lab_test_task, &see_doctor_task]() -> Task {
        auto allof = std::make_shared<AllOfEvent>(env, std::vector<std::shared_ptr<SimEvent>>{
            lab_test_task->get_completion_event(),
            see_doctor_task->get_completion_event()
        });
        co_await *allof;
        std::cout << "[" << env.sim_time << "] patient signs out\n";
    });

    env.schedule(register_task, "register");
    env.schedule(see_doctor_task, "see_doctor");
    env.schedule(lab_test_task, "lab_test");
    env.schedule(signout_task, "signout");

    env.run();
}

int main() {
    example_porcess_interrupt();
};
