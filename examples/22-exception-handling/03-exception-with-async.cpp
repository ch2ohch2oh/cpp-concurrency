#include <future>
#include <iostream>

int async_worker() {
    throw std::runtime_error("Async error");
    return 42;
}

int main() {
    auto fut = std::async(std::launch::async, async_worker);
    
    try {
        int result = fut.get();
        std::cout << "Result: " << result << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Caught exception from async: " << e.what() << "\n";
    }
    
    return 0;
}
