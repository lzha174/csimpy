# CSimpy

CSimPy is a lightweight C++20 coroutine-based discrete event simulation engine, inspired by SimPy. It is designed to model asynchronous workflows and resource contention in a SimPy-style API, bringing familiar concepts into the C++ ecosystem. The framework aims to give researchers and students a practical and fun way to explore discrete event simulation with modern C++20 coroutines.


---

## 📦 Core Components

### 1. `CSimpyEnv`
The simulation environment. It manages:
- `sim_time`: the current simulation clock
- `event_queue`: a priority queue of scheduled events.
- `schedule()`: inserts events into the queue based on their `sim_time`
- Advances simulation time and process events when triggered

### 2. `SimEvent`
Base class for events. Supports:
- Callback registration
- Coroutine suspension and resumption
- Manual or automatic triggering

### 3. `SimDelay`
A subclass of `SimEvent` for time-based delays.

### 4. `CoroutineProcess`
Wraps coroutine handles as simulation tasks. Scheduled in the event queue for resumption.

### 5. `AllOfEvent`
An event that waits on multiple other `SimEvent`s. Completes when all dependencies have triggered.

### 6. `AnyOfEvent`
An event that waits on multiple other `SimEvent`s. 
Completes when *any one* of the dependencies triggers. 

### 7. `Container`
A resource container supporting `put()` and `get()` operations with capacity constraints. 
Useful for modeling consumable or producible resources like inventory, fluids, or queues.

### 8. `Store`
A resource store for holding `ItemBase`-derived objects with limited capacity.
- Supports `put()` and `get()` operations using `co_await`.
- Items are managed as `std::shared_ptr<ItemBase>`.
- `get()` supports filter lambdas to select specific items.
- Both `put` and `get` now accept a `Priority` (e.g., `Priority::High` / `Priority::Low`) and higher priority waiters are serviced first. 
- Useful for modeling queues of objects such as staff, jobs, or inventory.

---

## 🔍 Features

- Event-driven execution with time advancement
- Automatic delay handling (`SimDelay`)
- Coroutine-based task modeling
- Support for composite events (like `AllOfEvent`)
- Event queue introspection (`print_event_queue_state()`)

---

## 🧪 Usage

### Simple Delay
```cpp
co_await SimDelay(env, 10);  // Pause for 10 units
```

### Wait for Another Process
```cpp
co_await proc_c.get_completion_event(), "wait_for_proc_c"};
```

### Wait on Multiple Events
```cpp
auto d1 = SimDelay(env, 5);
auto d2 = SimDelay(env, 10);
co_await AllOfEvent{env, {&d1, &d2}};
```

### Wait on Any Event
```cpp
auto d1 = SimDelay(env, 5);
auto d2 = SimDelay(env, 10);
co_await AnyOfEvent{env, {&d1, &d2}};
```

### Store Example
```cpp
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
```

---

## 🛠️ Workflow Example

Below is a sample usage demonstrating a patient flow with dependent tasks :

```cpp
#include "csimpy_env.h"

/**
 * Demonstrates a simple patient flow simulation:
 * 1. Patient registers (10 time units).
 * 2. After registration, the patient starts seeing a doctor and doing a lab test in parallel.
 * 3. Doctor consultation takes 20 time units.
 * 4. Lab test takes 40 time units.
 * 5. Patient signs out after both tasks are complete.
 */
void patient_flow() {
    CSimpyEnv env;

    auto register_task = env.create_task([&env]() -> Task {
        std::cout << "[" << env.sim_time << "] patient starts registration\n";
        co_await SimDelay(env, 10);
        std::cout << "[" << env.sim_time << "] patient finishes registration\n";
    });

    auto& reg_event = register_task->get_completion_event(); // capture once

    auto see_doctor_task = env.create_task([&env, &reg_event]() -> Task {
        co_await reg_event;
        std::cout << "[" << env.sim_time << "] patient starts seeing doctor\n";
        co_await SimDelay(env, 20);
        std::cout << "[" << env.sim_time << "] patient finishes seeing doctor\n";
    });

    auto lab_test_task = env.create_task([&env, &reg_event]() -> Task {
        co_await reg_event;
        std::cout << "[" << env.sim_time << "] patient starts lab test\n";
        co_await SimDelay(env, 40);
        std::cout << "[" << env.sim_time << "] patient finishes lab test\n";
    });

    auto signout_task = env.create_task([&env, &lab_test_task, &see_doctor_task]() -> Task {
        co_await AllOfEvent(env, {&lab_test_task->get_completion_event(), &see_doctor_task->get_completion_event()});
        std::cout << "[" << env.sim_time << "] patient signs out\n";
    });

    env.schedule(register_task, "register");
    env.schedule(see_doctor_task, "see_doctor");
    env.schedule(lab_test_task, "lab_test");
    env.schedule(signout_task, "signout");

    env.run();
}

int main() {
    patient_flow();
};
```

**Output:**
```
[0] patient starts registration
[10] patient finishes registration
[10] patient starts seeing doctor
[10] patient starts lab test
[30] patient finishes seeing doctor
[50] patient finishes lab test
[50] patient signs out
```

---

## 📈 TODO

- Support process cancellation and interruption
- For more examples, see `examples.cpp` in the source repository.
