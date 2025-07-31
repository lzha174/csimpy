#include "csimpy_env.h"
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
