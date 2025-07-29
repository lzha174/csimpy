//
// Created by ozhang on 20/07/2025.
//

#ifndef CSIMPY_CONTAINER_H
#define CSIMPY_CONTAINER_H


constexpr bool DEBUG_RESOURCE = false;
// Awaitable for simulating a timed delay
#include <iostream>
#include <coroutine>
#include "csimpy_env.h"

struct ContainerBase {
    virtual void put(int value) = 0;
    virtual bool can_get(int value) const = 0;
    virtual void get(int value) = 0;
    virtual ~ContainerBase() = default;
};

struct Container : ContainerBase {
    CSimpyEnv& env;
    int level = 0;
    int capacity;
    std::vector<std::pair<std::coroutine_handle<>, int>> get_waiters;
    std::vector<std::pair<std::coroutine_handle<>, int>> put_waiters;
    std::string name;

    Container(CSimpyEnv& e, int cap, std::string n = "") : env(e), capacity(cap), name(std::move(n)) {}

    bool can_put(int value) const {
        return level + value <= capacity;
    }
    bool can_get(int value) const override {
        return level >= value;
    }

    void await_get(std::coroutine_handle<> h, int value) {
        get_waiters.emplace_back(h, value);
    }

    void await_put(std::coroutine_handle<> h, int value) {
        put_waiters.emplace_back(h, value);
    }

    void try_wake_get_waiters() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Get Waiters (before trying):\n";
            for (const auto& [h, v] : get_waiters) {
                std::cout << "[" << name << "]   - wants: " << v << "\n";
            }
        }
        for (size_t i = 0; i < get_waiters.size();) {
            auto [h, v] = get_waiters[i];
            if (can_get(v)) {
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - try get : " << v << "\t"<<"level b4:"<<level<<"\n";
                }
                level -= v;
                if (DEBUG_RESOURCE) {
                    std::cout << "[" << name << "]   - tre get : " << v << "\t"<<"level after:"<<level<<"\n";
                }
                env.event_queue.push(new CoroutineProcess(env.sim_time, h, "Container::resume get"));

                get_waiters.erase(get_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void try_wake_put_waiters() {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "] 🔍 Put Waiters (before trying):\n";
            for (const auto& [h, v] : put_waiters) {
                std::cout << "[" << name << "]   - wants to put: " << v << "\n";
            }
        }
        for (size_t i = 0; i < put_waiters.size();) {
            auto [h, v] = put_waiters[i];
            if (can_put(v)) {
                level += v;
                env.event_queue.push(new CoroutineProcess(env.sim_time, h, "Container::resume put"));
                put_waiters.erase(put_waiters.begin() + i);
            } else {
                ++i;
            }
        }
    }

    void put(int value) override {
        if (can_put(value)) {
            if (DEBUG_RESOURCE) {
                std::cout << "[" << name << "]   - put : " << value << "\t"<<"level before :" << level << "\n";
            }
            level += value;
            if (DEBUG_RESOURCE) {
                std::cout << "[" << name << "]   - put : " << value << "\t"<<"level  after:" << level << "\n";
            }
            try_wake_get_waiters();  // notify get waiters
        } else {
            // Queue if over capacity, assume caller will suspend
        }
    }

    void get(int value) override {
        if (DEBUG_RESOURCE) {
            std::cout << "[" << name << "]   - get : " << value << "\t"<<"level :" << level << "\n";
        }
        level -= value;
        try_wake_put_waiters();  // notify put waiters
    }
};

struct ContainerPutEventOld {
    Container& container;
    int value;

    ContainerPutEventOld(Container& c, int v) : container(c), value(v) {}

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        container.await_put(h, value);
        container.try_wake_put_waiters();
    }

    void await_resume() const noexcept {
        container.try_wake_get_waiters();
    }
};






struct ContainerPutEvent : SimEventBase {
    CSimpyEnv& env;
    Container& container;
    int value;
    ContainerPutEvent(CSimpyEnv& env_, Container& c, int v) : env(env_), container(c), value(v) {
        sim_time = env.sim_time;
    }
    bool await_ready() const noexcept {
        // return false
        // queue this event
        return false;
    };
    void await_suspend(std::coroutine_handle<> h) const {
        //container.await_put(h, value);
        //container.try_wake_put_waiters();
    }
};

struct ContainerGetEventOld {
    Container& container;
    int value;

    ContainerGetEventOld(Container& c, int v) : container(c), value(v) {}

    bool await_ready() const noexcept {
        return false;
    }

    void await_suspend(std::coroutine_handle<> h) const {
        container.await_get(h, value);
        container.try_wake_get_waiters();  // try to fulfill get immediately
    }

    void await_resume() const noexcept {
        container.try_wake_put_waiters();
    }
};



#endif //CSIMPY_CONTAINER_H
