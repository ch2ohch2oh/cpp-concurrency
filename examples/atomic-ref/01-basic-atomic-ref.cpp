#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    int shared_value = 0;
    std::atomic_ref<int> ref(shared_value);
    
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            ref.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {
            ref.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    t1.join();
    t2.join();
    
    std::cout << "Final value: " << shared_value << "\n";
    return 0;
}
