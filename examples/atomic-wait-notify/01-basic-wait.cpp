#include <atomic>
#include <thread>
#include <iostream>

int main() {
    std::atomic<bool> ready{false};
    
    std::thread waiter([&]() {
        while (!ready.load()) {
            ready.wait(false);
        }
        std::cout << "Notified!\n";
    });
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    ready.store(true);
    ready.notify_one();
    
    waiter.join();
    return 0;
}
