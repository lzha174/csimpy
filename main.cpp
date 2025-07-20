#include "csimpy_env.h"
#include "csimpy_delay.h"
#include "csimpy_container.h"
#include <iostream>



void example_1() {
    CSimpyEnv env;

    Task proc_c = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] process_c started\n";
        co_await SimDelay(env, 15);
        std::cout << "[" << env.sim_time << "] process_c finished\n";
    });

    Task proc_a = env.create_task([&env, &proc_c]() -> Task {
       std::cout << "[" << env.sim_time << "] process_a started\n";
       co_await SimDelay(env, 5);
       std::cout << "[" << env.sim_time << "] process_a now waiting on process_c\n";
       co_await LabeledAwait{proc_c.get_completion_event(), "process_a"};
       std::cout << "[" << env.sim_time << "] process_a resumed after process_c\n";
   });

    Task proc_b = env.create_task([&env, &proc_c]() -> Task {
       std::cout << "[" << env.sim_time << "] process_b started\n";
       co_await SimDelay(env, 10);
       std::cout << "[" << env.sim_time << "] process_b now waiting on process_c\n";
       co_await LabeledAwait{proc_c.get_completion_event(), "process_b"};
       std::cout << "[" << env.sim_time << "] process_b resumed after process_c\n";
   });

    env.schedule(proc_c, "process_c");
    env.schedule(proc_b, "process_b");
    env.schedule(proc_a, "process_a");



    env.run();
}

void example_2() {
    CSimpyEnv env;
    Container test_container(env,15, "test_container");
    Task test_put_first = env.create_task([&env, &test_container]()  -> Task {
        co_await SimDelay(env, 5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 4\n";
        co_await ContainerPutEvent(test_container, 4);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";

        co_await SimDelay(env,5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 10\n";
        co_await ContainerPutEvent(test_container, 10);
        std::cout << "[" << env.sim_time << "] test_put_first: done\n";
   });

    Task test_get_second = env.create_task([&env, &test_container]() -> Task {
        co_await SimDelay(env,6);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 3"<< " current level  "<<test_container.level << std::endl;;
        co_await ContainerGetEvent(test_container, 3);
        std::cout << "[" <<  env.sim_time << "] test_get_second: got 3"<< " current level  "<<test_container.level << std::endl;;

        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 9"<< " current level  "<<test_container.level << std::endl;
        co_await ContainerGetEvent(test_container, 9);
        std::cout << "[" << env.sim_time << "] test_get_second: got 9" << " current level  "<<test_container.level << std::endl;
   });
    env.schedule(test_put_first, "test_put_first");
    env.schedule(test_get_second, "test_get_second");



    env.run();

}
int main() {
    example_1();
    example_2();
    return 0;
}