#include <future>
#include <chrono>
#include <iostream>

int main() {
    std::future<int> fut = std::async(std::launch::async, [] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 42;
    });
    
    if (fut.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
        std::cout << "Result: " << fut.get() << "\n";
    } else {
        std::cout << "Timeout\n";
    }
    
    return 0;
}
