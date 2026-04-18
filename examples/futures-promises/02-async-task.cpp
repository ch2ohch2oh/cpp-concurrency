#include <future>
#include <iostream>
#include <chrono>

int calculate() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 42;
}

int main() {
    // Launch async task
    std::future<int> fut = std::async(std::launch::async, calculate);
    
    // Do other work while task runs
    std::cout << "Task is running...\n";
    
    // Get result (blocks if not ready)
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    return 0;
}
