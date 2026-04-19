#include <thread>
#include <iostream>
#include <functional>

class Executor {
public:
    template<typename F>
    void execute(F&& f) {
        std::thread([f = std::forward<F>(f)]() mutable {
            f();
        }).detach();
    }
};

int main() {
    Executor executor;
    
    executor.execute([]() {
        std::cout << "Task executed\n";
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
