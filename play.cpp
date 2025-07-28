#include "csimpy_env.h"
CSimpyEnv env;
auto wait_example = env.create_task([]() -> Task {
    std::cout << "[" << env.sim_time << "] wait_example started\n";
    co_await SimDelay(env, 15);
    std::cout << "[" << env.sim_time << "] wait_example finished\n";
});

int main() {
    env.schedule(wait_example, "wait_example");
    env.run();
    return 0;
}