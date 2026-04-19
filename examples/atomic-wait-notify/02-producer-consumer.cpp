#include <atomic>
#include <thread>
#include <queue>
#include <iostream>
#include <mutex>

int main() {
    std::atomic<int> data{0};
    std::atomic<bool> ready{false};
    
    std::thread consumer([&]() {
        while (true) {
            ready.wait(false);
            if (data.load() == -1) break;
            std::cout << "Consumed: " << data.load() << "\n";
            ready.store(false);
        }
    });
    
    for (int i = 0; i < 5; ++i) {
        data.store(i);
        ready.store(true);
        ready.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    data.store(-1);
    ready.store(true);
    ready.notify_one();
    consumer.join();
    
    return 0;
}
