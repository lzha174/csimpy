#include "examples.h"
#include "csimpy_env.h" // or whatever core sim headers you use
#include "csimpy_container.h"
#include "examples.h"
#include "csimpy_env.h" // or whatever core sim headers you use
#include "csimpy_container.h"

// Global environment accessor
CSimpyEnv& get_global_env() {
    static CSimpyEnv env;
    return env;
}

void example_1() {
    CSimpyEnv env;

    // Create tasks using shared_ptr
    auto process_c = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] process_c started\n";
        co_await SimDelay(env, 15);
        std::cout << "[" << env.sim_time << "] process_c finished\n";
        co_return;
    });

    auto process_b = env.create_task([&env, &process_c]() -> Task {
        std::cout << "[" << env.sim_time << "] process_b started\n";
        co_await process_c->get_completion_event();
        std::cout << "[" << env.sim_time << "] process_b finished\n";
        co_return;
    });

    auto process_a = env.create_task([&env, &process_b]() -> Task {
        std::cout << "[" << env.sim_time << "] process_a started\n";
        co_await process_b->get_completion_event();
        std::cout << "[" << env.sim_time << "] process_a finished\n";
        co_return;
    });

    // Schedule tasks by passing shared_ptr
    env.schedule(process_c, "process_c");
    env.schedule(process_b, "process_b");
    env.schedule(process_a, "process_a");

    env.run();
}
void example_2() {
    CSimpyEnv env;
    Container test_container(env, 15, "test_container");
    auto test_put_first = env.create_task([&env, &test_container]()  -> Task {
        co_await SimDelay(env, 5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 4\n";
        co_await ContainerPutEvent(test_container, 4);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";

        co_await SimDelay(env,5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 10\n";
        co_await ContainerPutEvent(test_container, 10);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";
    });

    auto test_get_second = env.create_task([&env, &test_container]() -> Task {
        co_await SimDelay(env,6);  // wait 6 units
        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 3"<< " current level  "<<test_container.level << std::endl;
        co_await ContainerGetEvent(test_container, 3);
        std::cout << "[" <<  env.sim_time << "] test_get_second: got 3"<< " current level  "<<test_container.level << std::endl;

        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 9"<< " current level  "<<test_container.level << std::endl;
        co_await ContainerGetEvent(test_container, 9);
        std::cout << "[" << env.sim_time << "] test_get_second: got 9" << " current level  "<<test_container.level << std::endl;
    });
    env.schedule(test_put_first, "test_put_first");
    env.schedule(test_get_second, "test_get_second");
    env.run();
}


void example_3() {
    CSimpyEnv env;
    auto proc_all_wait = env.create_task([&env]() -> Task {
        SimDelay d1(env, 5);
        SimDelay d2(env, 10);
        co_await AllOfEvent{env, {&d1, &d2}};
        std::cout << "[" << env.sim_time << "] All delays finished.\n";
    });
    env.schedule(proc_all_wait, "proc_all_wait");
    env.run();
}

void example_4() {
    CSimpyEnv env;
    // Shared event
    auto shared_event = std::make_shared<SimEvent>(env);
    auto shared_event_1 = std::make_shared<SimEvent>(env);
    auto task1 = env.create_task([&env, &shared_event, &shared_event_1]() -> Task {
        co_await SimDelay(env, 1);
        std::cout << "[" << env.sim_time << "] task1 waiting on shared_event or timeout\n";
        auto timeout = SimDelay(env, 5);
        co_await AllOfEvent{env, {&timeout, shared_event.get(), shared_event_1.get()}};
        std::cout << "[" << env.sim_time << "] task1 finished waiting (timeout and event)\n";
    });
    auto task2 = env.create_task([&env, &shared_event, &shared_event_1]() -> Task {
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] task2 triggering shared_event\n";
        shared_event->on_succeed();
        shared_event_1->on_succeed();
    });
    env.schedule(task1, "task1");
    env.schedule(task2, "task2");
    env.run();
}

// Example: Patient flow simulation
void example_patient_flow() {
    CSimpyEnv env;

    auto register_task = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] patient starts registration\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] patient finishes registration\n";
    });

    auto& reg_event = register_task->get_completion_event(); // capture once

    auto see_doctor_task = env.create_task([&env, &reg_event]() -> Task {
        co_await LabeledAwait{reg_event, "see_doctor"};
        std::cout << "[" << env.sim_time << "] patient starts seeing doctor\n";
        co_await SimDelay(env, 20);
        std::cout << "[" << env.sim_time << "] patient finishes seeing doctor\n";
    });

    auto lab_test_task = env.create_task([&env, &reg_event]() -> Task {
        co_await LabeledAwait{reg_event, "lab_test"};
        std::cout << "[" << env.sim_time << "] patient starts lab test\n";
        co_await SimDelay(env, 40);
        std::cout << "[" << env.sim_time << "] patient finishes lab test\n";
    });

    auto& doc_event = see_doctor_task->get_completion_event();
    auto& lab_event = lab_test_task->get_completion_event();

    auto signout_task = env.create_task([&env, &lab_event, &see_doctor_task]() -> Task {
        auto  x =3;
        co_await AllOfEvent(env, {&lab_event, &see_doctor_task->get_completion_event()});
        std::cout << "[" << env.sim_time << "] patient signs out\n";
    });

    env.schedule(register_task, "register");
    env.schedule(see_doctor_task, "see_doctor");
    env.schedule(lab_test_task, "lab_test");
    env.schedule(signout_task, "signout");

    env.run();
}