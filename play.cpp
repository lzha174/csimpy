#include "csimpy_env.h"
CSimpyEnv env;

Store store(env, 5);

auto task = env.create_task([]() -> Task {
    StaffItem staff1("Alice", 1, "Nurse", 2);
    StaffItem staff2("Bob", 2, "Doctor", 3);

    std::cout << "[" << env.sim_time << "] Putting Alice\n";
    co_await store.put(staff1);
    std::cout << "[" << env.sim_time << "] Putting Bob\n";
    co_await store.put(staff2);

    std::cout << "[" << env.sim_time << "] Getting item with id == 2\n";
    auto filter = [](const std::shared_ptr<ItemBase>& item) {
        return item->id == 2;
    };
    auto val = co_await store.get(filter);
    std::cout << "[" << env.sim_time << "] Got item with id == "
              << std::get<std::string>(val) << std::endl;

    std::cout << "[" << env.sim_time << "] Getting next available item (no filter)\n";
    auto next_val = co_await store.get();
    std::cout << "[" << env.sim_time << "] Got item: "
              << std::get<std::string>(next_val) << std::endl;
});

int main() {
    env.schedule(task, "store_example");
    env.run();
};