#include <iostream>
#include <memory>

struct Task {
    Task(const std::string& name) : name(name) {
        std::cout << name << " created\n";
    }
    ~Task() {
        std::cout << name << " destroyed\n";
    }
    std::string name;
    void run() { std::cout << name << " running\n"; }
};

void use_task(std::shared_ptr<Task> task_ptr) {
    std::cout << "use_task() got shared_ptr. Count = " << task_ptr.use_count() << "\n";
    task_ptr->run();
}

int main() {
    std::shared_ptr<Task> taskA = std::make_shared<Task>("TaskA");
    std::cout << "After creation, Count = " << taskA.use_count() << "\n";

    // Pass the same task to a function
    use_task(taskA);
    std::cout << "After function call, Count = " << taskA.use_count() << "\n";

    // Create another shared_ptr pointing to the same object
    auto taskA_copy = taskA;
    std::cout << "Created another shared_ptr, Count = " << taskA.use_count() << "\n";

    taskA_copy->run();

    // When both shared_ptr go out of scope, the object is automatically deleted
    return 0;
}//
// Created by ozhang on 27/07/2025.
//
