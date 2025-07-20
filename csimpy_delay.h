#pragma once
#include "csimpy_env.h"

struct SimDelay {
    CSimpyEnv& env;
    int wake_time;
    SimDelay(CSimpyEnv& e, int delay) : env(e), wake_time(e.sim_time + delay) {
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const {
        env.schedule(new CoroutineProcess(wake_time, h, "SimDelay"));
    }

    void await_resume() const noexcept {}
};