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



/**
 * Example 1:
 * Demonstrates task dependencies and labeled awaits.
 * proc_a waits for proc_c, proc_b waits for proc_c and then both for AllOfEvent.
 */
void example_1() {
    CSimpyEnv env;

    auto proc_c = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] process_c started\n";
        co_await SimDelay(env, 15);
        std::cout << "[" << env.sim_time << "] process_c finished\n";
    });

    auto proc_a = env.create_task([&env, &proc_c]() -> Task {
        std::cout << "[" << env.sim_time << "] process_a started\n";
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] process_a now waiting on process_c\n";
        co_await LabeledAwait{proc_c->get_completion_event(), "process_a"};
        std::cout << "[" << env.sim_time << "] process_a resumed after process_c\n";
        co_await SimDelay(env, 25);
        std::cout << "[" << env.sim_time << "] process_a finished \n";
    });

    auto proc_b = env.create_task([&env, &proc_c, &proc_a]() -> Task {
        std::cout << "[" << env.sim_time << "] process_b started\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] process_b now waiting on process_c\n";
        co_await LabeledAwait{proc_c->get_completion_event(), "process_b"};
        std::cout << "[" << env.sim_time << "] process_b resumed after process_c\n";
        co_await AllOfEvent{env, {&proc_c->get_completion_event(), &proc_a->get_completion_event()}};
        std::cout << "[" << env.sim_time << "] proc_b finished waiting allofevent\n";
    });

    env.schedule(proc_c, "process_c");
    env.schedule(proc_b, "process_b");
    env.schedule(proc_a, "process_a");

    env.run();
}
/**
 * Example 2:
 * Demonstrates Container usage with put and get operations.
 * Shows blocking get until sufficient resources are available.
 */
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


/**
 * Example 3:
 * Demonstrates AllOfEvent with multiple SimDelay events.
 * Waits until all delays are complete before proceeding.
 */
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

/**
 * Example 4:
 * Demonstrates shared events and combining them with timeout using AllOfEvent.
 */
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

/**
 * Patient Flow Example:
 * Models a simple patient workflow:
 * 1. Registration (10 units).
 * 2. After registration, doctor and lab test start in parallel (20 and 40 units).
 * 3. Signs out after both are complete.
 */
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

    auto signout_task = env.create_task([&env, &lab_test_task, &see_doctor_task]() -> Task {
        auto  x =3;
        co_await AllOfEvent(env, {&lab_test_task->get_completion_event(), &see_doctor_task->get_completion_event()});
        std::cout << "[" << env.sim_time << "] patient signs out\n";
    });

    env.schedule(register_task, "register");
    env.schedule(see_doctor_task, "see_doctor");
    env.schedule(lab_test_task, "lab_test");
    env.schedule(signout_task, "signout");

    env.run();
}

/**
 * Example 5:
 * Demonstrates AnyOfEvent waiting for the first of multiple delays to complete.
 */
void example_5() {
    CSimpyEnv env;
    auto proc_any_wait = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] proc_any_wait started\n";
        SimDelay d1(env, 5);
        SimDelay d2(env, 10);
        co_await AnyOfEvent{env, {&d1, &d2}};
        std::cout << "[" << env.sim_time << "] AnyOfEvent triggered after one delay\n";
    });
    env.schedule(proc_any_wait, "proc_any_wait");
    env.run();
}

/**
 * Example 6:
 * Demonstrates AnyOfEvent combining a task completion event and a SimDelay.
 */
void example_6() {
    CSimpyEnv env;

    auto proc_a = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] proc_a started\n";
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] proc_a finished\n";
    });

    auto proc_b = env.create_task([&env, &proc_a]() -> Task {
        std::cout << "[" << env.sim_time << "] proc_b started\n";
        SimDelay d1(env, 10);
        std::cout << "[" << env.sim_time << "] proc_b waiting on proc_a or 10 delay\n";
        co_await AnyOfEvent{env, {&proc_a->get_completion_event(), &d1}};
        std::cout << "[" << env.sim_time << "] proc_b resumed after AnyOfEvent\n";
    });

    env.schedule(proc_b, "proc_b");
    env.schedule(proc_a, "proc_a");

    env.run();
}

/**
 * Example 7:
 * Demonstrates dynamic scheduling of tasks inside another coroutine after a delay.
 */
void example_7() {
    CSimpyEnv env;
    auto proc_a = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] proc_a started\n";
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] proc_a finished\n";
    });
    auto proc_b = env.create_task([&env, &proc_a]() -> Task {
        std::cout << "[" << env.sim_time << "] proc_b started\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] proc_b finished delay, now scheduling proc_a\n";

        env.schedule(proc_a, "proc_a");
    });

    env.schedule(proc_b, "proc_b");

    env.run();
}
