#include "../../include/examples/simsettings.h"
#include "../../include/examples/examples.h"
#include "../../include/csimpy/csimpy_env.h"
#include "../../include/examples/staffitem.h"
#include "../../include/examples/EDstaff.h"

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
        co_await LabeledAwait{*proc_c->get_completion_event(), "trigger process_a"};
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
    //env.schedule(proc_b, "process_b");
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
        auto d1 = std::make_shared<SimDelay>(env, 5);
        auto d2 = std::make_shared<SimDelay>(env, 10);
        co_await AllOfEvent{env, {d1, d2}};
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
        auto timeout = std::make_shared<SimDelay>(env, 5);
        co_await AllOfEvent{env, {timeout, shared_event, shared_event_1}};
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
        auto d1 = std::make_shared<SimDelay>(env, 5);
        auto d2 = std::make_shared<SimDelay>(env, 10);
        co_await AnyOfEvent{env, {d1, d2}};
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

/**
 * Example: Demonstrates interrupting a task and catching the InterruptException.
 */
void example_interrupt() {
    CSimpyEnv env;

    // Create a worker task that can be interrupted during a long delay.
    auto worker = env.create_task([&env]() -> Task {
        try {
            std::cout << "[" << env.sim_time << "] worker: starting long delay\n";
            co_await SimDelay(env, 20);
            std::cout << "[" << env.sim_time << "] worker: finished long delay (not interrupted)\n";
        } catch (const InterruptException& ex) {
            std::cout << "[" << env.sim_time << "] worker: interrupted! Cause: ";
            co_return;
        }
    });

    // Create a controller task that interrupts the worker after a short delay.
    auto controller = env.create_task([&env, &worker]() -> Task {
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] controller: interrupting worker\n";
        // You can pass a cause (e.g. a MapItem or nullptr)
        worker->interrupt(std::make_shared<SimpleItem>("urgent_call", 1));
        std::cout << "[" << env.sim_time << "] controller: worker interrupted\n";
    });

    env.schedule(worker, "worker");
    env.schedule(controller, "worker");
    env.run();
}


/**
 * Example: Demonstrates interrupting a task waiting on a shared event.
 */
void example_event_interrupt() {
    CSimpyEnv env;

    auto shared_event = std::make_shared<SimEvent>(env);

    // Worker waits on the shared_event
    auto worker = env.create_task([&env, &shared_event]() -> Task {
        try {
            std::cout << "[" << env.sim_time << "] worker: waiting on shared_event\n";
            co_await *shared_event;
            std::cout << "[" << env.sim_time << "] worker: shared_event succeeded\n";
        } catch (const InterruptException& ex) {
            std::cout << "[" << env.sim_time << "] worker: interrupted while waiting, cause: ";
            if (ex.cause) {
                std::cout << ex.cause->to_string() << "\n";
            } else {
                std::cout << "(none)\n";
            }
        }
    });

    // Controller interrupts the worker after 5 time units
    auto controller = env.create_task([&env, &worker]() -> Task {
        co_await SimDelay(env, 5);
        std::cout << "[" << env.sim_time << "] controller: interrupting worker (waiting on event)\n";
        worker->interrupt(std::make_shared<SimpleItem>("timeout_interrupt", 2));
        std::cout << "[" << env.sim_time << "] controller: worker interrupted\n";
    });

    env.schedule(worker, "worker_event_wait");
    env.schedule(controller, "controller_event_interrupt");

    env.run();
}