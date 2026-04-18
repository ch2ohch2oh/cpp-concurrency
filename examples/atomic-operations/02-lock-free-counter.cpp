#include <atomic>
#include <vector>
#include <thread>
#include <iostream>

class LockFreeCounter {
private:
    std::atomic<uint64_t> count_;
    
public:
    LockFreeCounter() : count_(0) {}
    
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    uint64_t get() const {
        return count_.load(std::memory_order_relaxed);
    }
};

int main() {
    LockFreeCounter counter;
    const int num_threads = 8;
    const int increments_per_thread = 1000000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread] {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter.increment();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final count: " << counter.get() << "\n";
    return 0;
}
