#include <thread>
#include <iostream>

void throwing_function() {
    throw std::runtime_error("Error in thread");
}

int main() {
    std::thread t([]() {
        try {
            throwing_function();
        } catch (const std::exception& e) {
            std::cerr << "Caught exception in thread: " << e.what() << "\n";
        }
    });
    
    t.join();
    return 0;
}
