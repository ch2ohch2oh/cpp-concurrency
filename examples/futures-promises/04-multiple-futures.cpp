#include <future>
#include <vector>
#include <iostream>
#include <chrono>

int main() {
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            return i * i;
        }));
    }
    
    // Wait for all futures
    for (auto& fut : futures) {
        std::cout << "Result: " << fut.get() << "\n";
    }
    
    return 0;
}
