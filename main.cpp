#include "csimpy_env.h"
#include "csimpy_delay.h"
#include <iostream>

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
   });

    Task proc_b = env.create_task([&proc_c]() -> Task {
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

int main() {
    example_1();
    return 0;
}