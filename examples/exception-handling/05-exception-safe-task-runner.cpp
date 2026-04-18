#include <thread>
#include <future>
#include <vector>
#include <iostream>

class TaskRunner {
private:
    std::vector<std::future<void>> futures_;
    
public:
    template<class F>
    void run(F&& f) {
        auto promise = std::make_shared<std::promise<void>>();
        futures_.push_back(promise->get_future());
        
        std::thread([promise, f = std::forward<F>(f)]() mutable {
            try {
                f();
                promise->set_value();
            } catch (const std::exception& e) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
    }
    
    void wait_all() {
        for (auto& fut : futures_) {
            try {
                fut.get();
            } catch (const std::exception& e) {
                std::cerr << "Task failed: " << e.what() << "\n";
            }
        }
        futures_.clear();
    }
    
    bool has_exceptions() {
        for (auto& fut : futures_) {
            try {
                fut.wait();
            } catch (...) {
                return true;
            }
        }
        return false;
    }
};

int main() {
    TaskRunner runner;
    
    // Run some tasks
    runner.run([]() {
        std::cout << "Task 1 running\n";
        std::cout << "Task 1 completed\n";
    });
    
    runner.run([]() {
        std::cout << "Task 2 running\n";
        throw std::runtime_error("Task 2 encountered an error");
    });
    
    runner.run([]() {
        std::cout << "Task 3 running\n";
        std::cout << "Task 3 completed\n";
    });
    
    runner.run([]() {
        std::cout << "Task 4 running\n";
        throw std::logic_error("Task 4 logic error");
    });
    
    std::cout << "Waiting for all tasks to complete...\n";
    runner.wait_all();
    
    std::cout << "All tasks completed\n";
    
    return 0;
}
