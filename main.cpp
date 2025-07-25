#include "csimpy_env.h"
//#include "examples.h"

CSimpyEnv env;
Task wait_example = env.create_task([]() -> Task {
    std::cout << "[" << env.sim_time << "] wait_example started\n";
    co_await SimDelay(env, 15);
    std::cout << "[" << env.sim_time << "] wait_example finished\n";
});

int main() {


     env.schedule(wait_example, "proc_c");
     env.run();
     return 0;
}