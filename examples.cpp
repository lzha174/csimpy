#include "examples.h"
#include "csimpy_env.h" // or whatever core sim headers you use
#include "csimpy_container.h"
CSimpyEnv env;

void example_1() {


    Task proc_c = env.create_task([]() -> Task {
        std::cout << "[" << env.sim_time << "] process_c started\n";
        co_await SimDelay(env, 15);
        std::cout << "[" << env.sim_time << "] process_c finished\n";
    });

    Task proc_a = env.create_task([&proc_c]() -> Task {
       std::cout << "[" << env.sim_time << "] process_a started\n";
       co_await SimDelay(env, 5);
       std::cout << "[" << env.sim_time << "] process_a now waiting on process_c\n";
       co_await LabeledAwait{proc_c.get_completion_event(), "process_a"};
       std::cout << "[" << env.sim_time << "] process_a resumed after process_c\n";
        co_await SimDelay(env, 25);

        std::cout << "[" << env.sim_time << "] process_a finished \n";
   });

    Task proc_b = env.create_task([&proc_c, &proc_a]() -> Task {
       std::cout << "[" << env.sim_time << "] process_b started\n";
       co_await SimDelay(env, 10);
       std::cout << "[" << env.sim_time << "] process_b now waiting on process_c\n";
       co_await LabeledAwait{proc_c.get_completion_event(), "process_b"};
       std::cout << "[" << env.sim_time << "] process_b resumed after process_c\n";
        co_await AllOfEvent{env, {&proc_c.get_completion_event(), &proc_a.get_completion_event()}};
          std::cout << "[" << env.sim_time << "] proc_b finished waiting allofevent\n";
   });

    env.schedule(proc_c, "process_c");
    env.schedule(proc_b, "process_b");
    env.schedule(proc_a, "process_a");



    env.run();
}

void example_2() {
    // Use global env
    Container test_container(env, 15, "test_container");
    Task test_put_first = env.create_task([&test_container]()  -> Task {
        co_await SimDelay(env, 5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 4\n";
        co_await ContainerPutEvent(test_container, 4);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";

        co_await SimDelay(env,5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 10\n";
        co_await ContainerPutEvent(test_container, 10);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";
   });

    Task test_get_second = env.create_task([&test_container]() -> Task {
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
    // Use global env
    Task proc_all_wait = env.create_task([]() -> Task {
        SimDelay d1(env, 5);
        SimDelay d2(env, 10);
        co_await AllOfEvent{env, {&d1, &d2}};
        std::cout << "[" << env.sim_time << "] All delays finished.\n";
    });
    env.schedule(proc_all_wait, "proc_all_wait");
    env.run();
}

void example_4() {
    // Use global env
    // Shared event
    auto shared_event = std::make_unique<SimEvent>(env);
    Task task1 = env.create_task([&shared_event]() -> Task {
        co_await SimDelay(env, 1);
        std::cout << "[" << env.sim_time << "] task1 waiting on shared_event or timeout\n";
        auto timeout = SimDelay(env, 5);
        co_await AllOfEvent{env, {&timeout, shared_event.get()}};
        std::cout << "[" << env.sim_time << "] task1 finished waiting (timeout and event)\n";
    });
    Task task2 = env.create_task([&shared_event]() -> Task {
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] task2 triggering shared_event\n";
        shared_event->on_succeed();
    });
    env.schedule(task1, "task1");
    env.schedule(task2, "task2");
    env.run();
}

// Example: Patient flow simulation
void example_patient_flow() {
    // Task: Register (10 time units)
    Task register_task = env.create_task([]() -> Task {
        std::cout << "[" << env.sim_time << "] patient starts registration\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] patient finishes registration\n";
    });

    // Task: See Doctor (20 time units), waits for registration
    Task see_doctor_task = env.create_task([&register_task]() -> Task {
        co_await LabeledAwait{register_task.get_completion_event(), "see_doctor"};
        std::cout << "[" << env.sim_time << "] patient starts seeing doctor\n";
        co_await SimDelay(env, 20);
        std::cout << "[" << env.sim_time << "] patient finishes seeing doctor\n";
    });

    // Task: Lab Test (40 time units), waits for registration
    Task lab_test_task = env.create_task([&register_task]() -> Task {
        co_await LabeledAwait{register_task.get_completion_event(), "lab_test"};
        std::cout << "[" << env.sim_time << "] patient starts lab test\n";
        co_await SimDelay(env, 40);
        std::cout << "[" << env.sim_time << "] patient finishes lab test\n";
    });

    // Task: Signout, waits for doctor and lab test
    Task signout_task = env.create_task([&see_doctor_task, &lab_test_task]() -> Task {
        co_await AllOfEvent(env, {
            &see_doctor_task.get_completion_event(),
            &lab_test_task.get_completion_event()
        });
        std::cout << "[" << env.sim_time << "] patient signs out\n";
    });

    // Schedule all tasks
    env.schedule(register_task, "register");
    env.schedule(see_doctor_task, "see_doctor");
    env.schedule(lab_test_task, "lab_test");
    env.schedule(signout_task, "signout");

    env.run();
}
