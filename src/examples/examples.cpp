#include "../../include/examples/simsettings.h"
#include "../../include/examples/examples.h"
#include "../../include/csimpy/csimpy_env.h"
#include "../../include/examples/staffitem.h"
#include "../../include/examples/EDstaff.h"
#include "../../include/examples/staffmanager.h"
#include "../../include/examples/job.h"
#include <memory>

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
        co_await LabeledAwait{*proc_c->get_completion_event(), "process_a"};
        std::cout << "[" << env.sim_time << "] process_a resumed after process_c\n";
        co_await SimDelay(env, 25);
        std::cout << "[" << env.sim_time << "] process_a finished \n";
    });

    auto proc_b = env.create_task([&env, &proc_c, &proc_a]() -> Task {
        std::cout << "[" << env.sim_time << "] process_b started\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] process_b now waiting on process_c\n";
        co_await *proc_c->get_completion_event();
        std::cout << "[" << env.sim_time << "] process_b resumed after process_c\n";
        co_await AllOfEvent{env, {proc_c->get_completion_event(), proc_a->get_completion_event()}};
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
        co_await test_container.put(4);
        std::cout << "[" << env.sim_time << "] test_put_first: done current level " << test_container.level << std::endl ;

        co_await SimDelay(env,5);  // wait 5 units
        std::cout << "[" << env.sim_time << "] test_put_first: putting 10\n";
        co_await test_container.put(10);
        std::cout << "[" << env.sim_time << "] test_put_first: done current level " << test_container.level << std::endl ;
    });

    auto test_get_second = env.create_task([&env, &test_container]() -> Task {
        co_await SimDelay(env,6);  // wait 6 units
        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 3"<< " current level  "<<test_container.level << std::endl;
        co_await test_container.get(3);
        std::cout << "[" <<  env.sim_time << "] test_get_second: got 3"<< " current level  "<<test_container.level << std::endl;

        std::cout << "[" << env.sim_time << "] test_get_second: trying to get 9"<< " current level  "<<test_container.level << std::endl;
        co_await test_container.get(9);
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
        co_await AllOfEvent{env, {shared_event, shared_event_1}};
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
        co_await AllOfEvent(env, {lab_test_task->get_completion_event(), see_doctor_task->get_completion_event()});
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
        auto d1 = std::make_shared<SimDelay>(env, 10);
        std::cout << "[" << env.sim_time << "] proc_b waiting on proc_a or 10 delay\n";
        co_await AnyOfEvent{env, {proc_a->get_completion_event(), d1}};
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


/**
 * Example 8:
 * Demonstrates Store usage with putting items and retrieving one using a filter by ID.
 */
void example_8() {
    CSimpyEnv env;
    Store store(env, 5, "staff_store");

    auto test_task = env.create_task([&env, &store]() -> Task {
        co_await SimDelay(env, 1);

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
        auto val = co_await store.get(std::make_shared<std::function<bool(const std::shared_ptr<ItemBase>&)>>(filter));
        {

            std::cout << "[" << env.sim_time << "] Got item with id == " << val->to_string() << std::endl;
        }

        std::cout << "[" << env.sim_time << "] Getting next available item (no filter)\n";
        auto next_val = co_await store.get(std::make_shared<std::function<bool(const std::shared_ptr<ItemBase>&)>>([](const std::shared_ptr<ItemBase>&){ return true; }));
        {
            std::cout << "[" << env.sim_time << "] Got item: " << next_val->to_string() << std::endl;
        }
    });

    env.schedule(test_task, "test_task");
    env.run();
}

/**
 * Example: Demonstrates Store usage with priority put/get.
 */
void example_priority_store() {
    CSimpyEnv env;
    Store store(env, 2, "priority_store");

    auto producer = env.create_task([&env, &store]() -> Task {
        co_await SimDelay(env, 10); // wait before putting
        StaffItem s1("A", 1, "Tech", 1);
        StaffItem s2("B", 2, "Tech", 1);
        std::cout << "[" << env.sim_time << "] producer: putting two items\n";
        co_await store.put(s1);
        co_await store.put(s2);
    });

    auto low_getter = env.create_task([&env, &store]() -> Task {
        std::cout << "[" << env.sim_time << "] low_getter: trying to get low priority item immediately\n";
        auto val = co_await store.get({}, Priority::Low);
        std::cout << "[" << env.sim_time << "] low_getter: got " << val->to_string() << "\n";
    });

    auto high_getter = env.create_task([&env, &store]() -> Task {
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] high_getter: trying to get high priority item at time 5\n";
        auto val = co_await store.get({}, Priority::High);
        std::cout << "[" << env.sim_time << "] high_getter: got " <<val->to_string() << "\n";
    });

    env.schedule(producer, "producer");
    env.schedule(low_getter, "low_getter");
    env.schedule(high_getter, "high_getter");
    env.run();
}



// Carwash example using Container as machines (capacity=2).
// Initial 4 cars arrive at time 0; then 5 additional cars arrive every 5 units.
// Service (wash) time is fixed at 10 units. FIFO queuing is used so waiting cars
// are served in arrival order. Logs arrival, entry, and exit times.
// Check carwash.py for equivalent simpy code.
void example_carwash_with_container() {
    CSimpyEnv env;
    // Use Container as machines with capacity 2
    Container carwash(env, 2, "carwash_container");
    carwash.set_level(2);
    auto car_request = [&](const std::string& name) {
        return env.create_task([&env, &carwash, name]() -> Task {
            std::cout << "[" << env.sim_time << "] " << name << " arrives at the carwash." << std::endl;
            co_await carwash.get(1);
            std::cout << "[" << env.sim_time << "] " << name << " enters the carwash." << std::endl;
            co_await SimDelay(env, 10);
            std::cout << "[" << env.sim_time << "] " << name << " leaves the carwash." << std::endl;
            co_await carwash.put(1);
            co_return;
        });
    };


    // initial 4 cars
    for (int i = 0; i < 4; ++i) {
        std::string name = "Car " + std::to_string(i);
        env.schedule(car_request(name), name);
    }

    // Producer task: spawn 5 extra cars at intervals
    auto producer = env.create_task([&env, &car_request]() -> Task {
        int count = 4;
        for (int i = 0; i < 5; ++i) {
            co_await SimDelay(env, 5); // arrival every 5 units
            std::string name = "Car " + std::to_string(count++);
            env.schedule(car_request(name), name);
        }
    });
    env.schedule(producer, "producer");
    env.run();
}


/**
 * Simplified Gas Station example.
 * - Pumps: Container with capacity 2 (two simultaneous users).
 * - Fuel tank: Container with capacity 10, initially full.
 * - Two cars arrive deterministically: Car 0 at time 0, Car 1 at time 5. Each consumes 8 units of fuel.
 * - Every 8 time units a monitor checks the fuel tank; if its level is < 8, it schedules a tank truck
 *   to arrive in 3 units and refill the tank to full.
 *   check gas_statoin.py for mirrior simpy code.
 */
void example_gas_station() {
    CSimpyEnv env;

    // Pumps and fuel tank
    Container pumps(env, 2, "pumps");
    pumps.set_level(2);  // two pumps available
    Container fuel_tank(env, 10, "fuel_tank");
    fuel_tank.set_level(10); // start full

    const int CAR_FUEL_NEED = 8;
    const int CAR_ARRIVAL_INTERVAL = 5;
    const int CHECK_INTERVAL = 8;
    const int REFUEL_DELAY = 3;
    const int LOW_THRESHOLD = 8;
    const int NUM_CARS = 2;

    // Car process
    auto make_car = [&](const std::string& name, int index) {
        return env.create_task([&env, &pumps, &fuel_tank, name, index]() -> Task {
            co_await SimDelay(env, index * CAR_ARRIVAL_INTERVAL);
            std::cout << "[" << env.sim_time << "] " << name << " arrives at the gas station\n";

            // Acquire pump
            co_await pumps.get(1);
            std::cout << "[" << env.sim_time << "] " << name << " acquired a pump\n";

            // Take fuel
            co_await fuel_tank.get(CAR_FUEL_NEED);
            std::cout << "[" << env.sim_time << "] " << name << " refueled with " << CAR_FUEL_NEED << " units\n";

            // Release pump
            co_await pumps.put(1);
            std::cout << "[" << env.sim_time << "] " << name << " left the gas station\n";
        });
    };

    // Fuel monitor + truck
    auto tank_truck = [&]() {
        return env.create_task([&env, &fuel_tank]() -> Task {
            co_await SimDelay(env, REFUEL_DELAY); // arrives REFUEL_DELAY after scheduled
            int amount = fuel_tank.capacity - fuel_tank.level;
            co_await fuel_tank.put(amount);
            std::cout << "[" << env.sim_time << "] Tank truck arrived and refilled station with "
                      << amount << " units\n";
            co_return;
        });
    };

    auto monitor = env.create_task([&env, &fuel_tank, &tank_truck]() -> Task {
        const int MAX_TIME = 50; // or configurable
        while (env.sim_time <= MAX_TIME) {
            co_await SimDelay(env, CHECK_INTERVAL);
            if (fuel_tank.level < LOW_THRESHOLD) {
                std::cout << "[" << env.sim_time << "] Fuel low (level=" << fuel_tank.level
                          << "), scheduling truck in " << REFUEL_DELAY << "\n";
                env.schedule(tank_truck(), "tank_truck");
            }
        }
    });
    // Schedule cars
    for (int i = 0; i < NUM_CARS; ++i) {
        std::string name = "Car " + std::to_string(i);
        env.schedule(make_car(name, i), name);
    }
    env.schedule(monitor, "fuel_monitor");

    env.run();
}

// Demonstrates adding shifts to EDStaff and printing them with simulation time offsets.
// Example: assign “break tokens” (capacity 2) to staff during their breaks.
// A shared Store of capacity 2 models limited break slots. For each staff and each break:
//   * wait until the break start (by co_awaiting a SimDelay to that offset),
//   * co_await store.get(...) with a filter on staff name to acquire a slot,
//   * stay in break for its duration (SimDelay),
//   * then co_await store.put(...) to release the slot.
void example_staff_shifts() {
    using namespace std::chrono;

    // Simulation start: Aug 4 2025 00:00
    SimSettings settings(make_time(2025, 8, 4, 0, 0));
    std::cout << "Simulation reference start: " << settings.current_time_str(0) << "\n";

    // Create staff and assign shifts/breaks exactly like before
    EDStaff john("John", 1);
    EDStaff mike("Mike", 2);
    std::vector<Shift> john_shifts = {
        Shift{make_time(2025, 8, 4, 0, 0), make_time(2025, 8, 4, 2, 0)},
        Shift{make_time(2025, 8, 4, 9, 0), make_time(2025, 8, 4, 16, 0)},
        Shift{make_time(2025, 8, 5, 0, 0), make_time(2025, 8, 5, 2, 0)},
        Shift{make_time(2025, 8, 5, 9, 0), make_time(2025, 8, 5, 16, 0)}
    };
    std::vector<Shift> mike_shifts = {
        Shift{make_time(2025, 8, 4, 0, 0), make_time(2025, 8, 4, 2, 0)},
        Shift{make_time(2025, 8, 4, 9, 0), make_time(2025, 8, 4, 16, 0)},
        Shift{make_time(2025, 8, 5, 0, 0), make_time(2025, 8, 5, 2, 0)},
        Shift{make_time(2025, 8, 5, 9, 0), make_time(2025, 8, 5, 16, 0)}
    };
    john.add_shifts(john_shifts);
    mike.add_shifts(mike_shifts);
    john.update_breaks(settings.start_time);
    mike.update_breaks(settings.start_time);

    // Store modeling break slots, capacity 2
    CSimpyEnv env;
    Store break_store(env, 2, "break_store");
    auto init_slots = env.create_task([&env, &break_store, &john, &mike]() -> Task {
        // put initial staff into break_store using co_await
        co_await break_store.put(john);
        co_await break_store.put(mike);
        break_store.print_items();
    });
    env.schedule(init_slots, "init_break_slots");

    // Helper to create a break task for a given staff and break shift
    auto make_break_task = [&](EDStaff& staff, const Shift& brk) {
        return env.create_task([&env, &break_store, &staff, brk, &settings]() -> Task {
            // Wait until break start relative to simulation start
            int wait_minutes = settings.minutes_from_start(brk.start);
            if (wait_minutes > 0) {
                co_await SimDelay(env, wait_minutes);
            }

            {
                auto start_str = format_time(brk.start);
                auto end_str = format_time(brk.end);
                std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff.name << " request break: "
                          << start_str << " - " << end_str << "\n";
            }

            // Acquire a break slot: filter doesn't need to check name here since tokens are generic,
            // but if you want to tie token to staff name you can include that logic.
            auto filter = [&](const std::shared_ptr<ItemBase>& item) {
                return item->name == staff.name;
            };
            auto filter_ptr = std::make_shared<std::function<bool(const std::shared_ptr<ItemBase>&)>>(filter);
            auto val = co_await break_store.get(filter_ptr);

            std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff.name << " acquired break slot\n";

            // verify break started at expected time
            int expected_start = settings.minutes_from_start(brk.start);
            if (env.sim_time != expected_start) {
                std::cerr << "Warning: break start mismatch for " << staff.name << ": expected " << expected_start << " but got " << env.sim_time << "\n";
            }
            assert(env.sim_time == expected_start && "Break did not start at expected simulation time");

            // Stay on break for duration
            int break_duration = static_cast<int>(
                duration_cast<minutes>(brk.end - brk.start).count());
            if (break_duration > 0) {
                co_await SimDelay(env, break_duration);
            }
            // release the slot back
            co_await break_store.put(val);
            std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff.name << " end break\n";
        });
    };

    // Schedule break tasks for all staff breaks
    int john_idx = 0;
    for (const auto& br : john.get_breaks()) {
        env.schedule(make_break_task(john, br), "john_break_" + std::to_string(john_idx++));
    }
    int mike_idx = 0;
    for (const auto& br : mike.get_breaks()) {
        env.schedule(make_break_task(mike, br), "mike_break_" + std::to_string(mike_idx++));
    }

    env.run();
}



void example_ed_sim() {
    using namespace std::chrono;

    // Simulation start: Aug 4 2025 00:00
    SimSettings settings(make_time(2025, 8, 4, 0, 0));
    std::cout << "Simulation reference start: " << settings.current_time_str(0) << "\n";

    // Create staff and assign shifts/breaks exactly like before
    auto john = std::make_shared<EDStaff>("John", 1, Skill::Junior);
    auto mike = std::make_shared<EDStaff>("Mike", 2, Skill::Mid);
    std::vector<Shift> john_shifts = {
        Shift{make_time(2025, 8, 4, 0, 0), make_time(2025, 8, 4, 2, 0)},
        Shift{make_time(2025, 8, 4, 9, 0), make_time(2025, 8, 4, 16, 0)},
        Shift{make_time(2025, 8, 5, 0, 0), make_time(2025, 8, 5, 2, 0)},
        Shift{make_time(2025, 8, 5, 9, 0), make_time(2025, 8, 5, 16, 0)}
    };
    std::vector<Shift> mike_shifts = {
        Shift{make_time(2025, 8, 4, 0, 0), make_time(2025, 8, 4, 2, 0)},
        Shift{make_time(2025, 8, 4, 9, 0), make_time(2025, 8, 4, 16, 0)},
        Shift{make_time(2025, 8, 5, 0, 0), make_time(2025, 8, 5, 2, 0)},
        Shift{make_time(2025, 8, 5, 9, 0), make_time(2025, 8, 5, 16, 0)}
    };
    john->add_shifts(john_shifts);
    mike->add_shifts(mike_shifts);
    john->update_breaks(settings.start_time);
    mike->update_breaks(settings.start_time);

    // Create a staff manager.

    // build and populate staff manager
    StaffManager staff_manager;
    staff_manager.add_staff(john);
    staff_manager.add_staff(mike);

    // Store modeling break slots, capacity 2
    CSimpyEnv env;
    Store break_store(env, 2, "break_store");
    auto init_slots = env.create_task([&env, &break_store, &staff_manager]() -> Task {
        for (auto& staff : staff_manager.get_all_staff()) {
            auto staff_put =  break_store.put(staff);
            co_await staff_put;
        }
        break_store.print_items();
    });
    env.schedule(init_slots, "init_break_slots");

    // Helper to create a break task for a given staff and break shift
    auto make_break_task = [&](std::shared_ptr<EDStaff> staff, const Shift& brk) {
        return env.create_task([&env, &break_store, staff, brk, &settings]() -> Task {
            // Wait until break start relative to simulation start
            int wait_minutes = settings.minutes_from_start(brk.start);
            if (wait_minutes > 0) {
                co_await SimDelay(env, wait_minutes);
            }

            {
                auto start_str = format_time(brk.start);
                auto end_str = format_time(brk.end);
                std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff->name << " request break: "
                          << start_str << " - " << end_str << "\n";
            }

            // Acquire a break slot: filter doesn't need to check name here since tokens are generic,
            // but if you want to tie token to staff name you can include that logic.
            auto filter = [&](const std::shared_ptr<ItemBase>& item) {
                return item->name == staff->name;
            };
            auto filter_ptr = std::make_shared<std::function<bool(const std::shared_ptr<ItemBase>&)>>(filter);
            auto val = co_await break_store.get(filter_ptr);

            std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff->name << " acquired break slot\n";

            // verify break started at expected time
            int expected_start = settings.minutes_from_start(brk.start);
            if (env.sim_time != expected_start) {
                std::cerr << "Warning: break start mismatch for " << staff->name << ": expected " << expected_start << " but got " << env.sim_time << "\n";
            }
            assert(env.sim_time == expected_start && "Break did not start at expected simulation time");

            // Stay on break for duration
            int break_duration = static_cast<int>(
                duration_cast<minutes>(brk.end - brk.start).count());
            if (break_duration > 0) {
                co_await SimDelay(env, break_duration);
            }
            // release the slot back
            co_await break_store.put(val);
            std::cout << "[" << settings.current_time_str(env.sim_time) << "] " << staff->name << " end break\n";
        });
    };

    // Schedule break tasks for all staff in manager
    for (auto& staff : staff_manager.get_all_staff()) {
        const auto& breaks = staff->get_breaks();
        for (size_t i = 0; i < breaks.size(); ++i) {
            env.schedule(make_break_task(staff, breaks[i]),
                         staff->name + "_break_" + std::to_string(i));
        }
    }


    // Create a sample job arriving at Aug 4 2025, 09:00
    JobManager jobManager;
    Job job1(make_time(2025, 8, 4, 9, 0), Minutes(60));
    // request one Junior staff member
    job1.skill_request[Skill::Junior] = 1;
    job1.skill_request[Skill::Mid] = 1;
    jobManager.add_job(job1);
    // Print stored jobs
    for (const auto& j : jobManager.jobs()) {
        std::cout << "Job: " << j.to_string()  << std::endl;
    }

    // Dispatch job task
    auto job_dispatch = env.create_task([&env, &settings, &job1, &break_store]() -> Task {
        // Wait until the job's arrival time
        int dispatch_delay = settings.minutes_from_start(job1.arrive_time);
        if (dispatch_delay > 0) {
            co_await SimDelay(env, dispatch_delay);
        }
        std::cout << "[" << settings.current_time_str(env.sim_time) << "] Dispatching job: "
                  << job1.to_string() << std::endl;
        auto duration = job1.duration;
        int duration_min = static_cast<int>(duration.count());
        // Acquire staff required for the job
        std::vector<std::shared_ptr<SimEvent>> req_evts;
        std::vector<std::shared_ptr<ItemBase>> acquired_staff;
        for (const auto& [skill, count] : job1.skill_request) {
            for (int i = 0; i < count; ++i) {
                auto filter = [&](const std::shared_ptr<ItemBase>& item) {
                    auto st = std::dynamic_pointer_cast<EDStaff>(item);
                    return st && st->skill == skill;
                };
                auto filter_ptr = std::make_shared<std::function<bool(const std::shared_ptr<ItemBase>&)>>(filter);
                auto staff_get = break_store.get(filter_ptr);
                req_evts.push_back(staff_get);
                //auto val = co_await staff_get;
                //acquired_staff.push_back(val);
            }
        }
        auto result_event = co_await AllOfEvent{env, req_evts};
        auto result_map = std::dynamic_pointer_cast<MapItem>(result_event);
        if (result_map) {
            for (const auto& [id, val_any] : result_map->map_value) {
                auto item = std::any_cast<std::shared_ptr<ItemBase>>(val_any);
                acquired_staff.push_back(item);
            }
        }

        co_await SimDelay(env, duration_min);
        // Return staff to break_store
        for (auto& staff : acquired_staff) {
            std::cout<< "[" << settings.current_time_str(env.sim_time) << "] put staff back "<< staff->name<<std::endl;
            co_await break_store.put(staff);
        }
        std::cout << "[" << settings.current_time_str(env.sim_time) << "] Job done"<<std::endl;

    });
    env.schedule(job_dispatch, "job_dispatch");

    env.run();
}