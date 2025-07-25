# CSimpy

CSimPy is a lightweight C++20 coroutine-based discrete event simulation engine, inspired by SimPy. It is designed to model asynchronous workflows and resource contention in a SimPy-style API, bringing familiar concepts into the C++ ecosystem. The framework aims to give researchers and students a practical and fun way to explore discrete event simulation with modern C++20 coroutines.


---

## 📦 Core Components

### 1. `CSimpyEnv`
The simulation environment. It manages:
- `sim_time`: the current simulation clock
- `event_queue`: a priority queue of scheduled `SimEvent`s
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

---

## 🧪 Examples

### Simple Delay
```cpp
co_await SimDelay(env, 10);  // Pause coroutine for 10 units
```

### Wait for Another Process
```cpp
co_await LabeledAwait{proc_c.get_completion_event(), "wait_for_proc_c"};
```

### Wait on Multiple Events
```cpp
auto d1 = new SimDelay(env, 5);
auto d2 = new SimDelay(env, 10);
co_await AllOfEvent{env, {d1, d2}};
```

---

## 🔍 Features

- Event-driven execution with time advancement
- Automatic delay handling (`SimDelay`)
- Coroutine-based task modeling
- Support for composite events (like `AllOfEvent`)
- Event queue introspection (`print_event_queue_state()`)

---

## 🛠️ Usage

Compile with C++20 support. Example CMake:
```cmake
set(CMAKE_CXX_STANDARD 20)
```

Run `main.cpp` to see usage examples.

---

## 📈 TODO

- Add `AnyOfEvent`
- Add resource constraint models (`Store`, `Resource`)
- Improve memory management (`unique_ptr` everywhere)
- Coroutine lifecycle diagnostics

---

## 👨‍💻 Author

CSimpy is a handcrafted simulation engine designed to bring SimPy-like elegance to C++.

---