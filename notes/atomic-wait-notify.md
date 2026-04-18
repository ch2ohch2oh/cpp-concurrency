# Atomic Wait/Notify Mechanisms: Usage and Implementation

## Practical Usage

### Basic atomic::wait

```cpp
#include <atomic>
#include <thread>
#include <iostream>

int main() {
    std::atomic<int> value(0);
    
    std::thread waiter([&]() {
        std::cout << "Waiting for value to change...\n";
        value.wait(0);  // Wait until value != 0
        std::cout << "Value changed to: " << value.load() << "\n";
    });
    
    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        value.store(42);
        value.notify_one();  // Wake one waiter
    });
    
    waiter.join();
    notifier.join();
    
    return 0;
}
```

### atomic::wait with Predicate

```cpp
#include <atomic>
#include <thread>
#include <iostream>

int main() {
    std::atomic<int> counter(0);
    
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            counter.fetch_add(1, std::memory_order_release);
            counter.notify_one();
        }
    });
    
    std::thread consumer([&]() {
        int expected = 0;
        counter.wait(expected, [](int v) { return v >= 5; });
        std::cout << "Counter reached 5\n";
    });
    
    producer.join();
    consumer.join();
    
    return 0;
}
```

### atomic::notify_one

```cpp
#include <atomic>
#include <thread>
#include <vector>

int main() {
    std::atomic<bool> ready(false);
    
    std::vector<std::thread> waiters;
    for (int i = 0; i < 5; ++i) {
        waiters.emplace_back([&ready, i]() {
            ready.wait(false);
            std::cout << "Waiter " << i << " woke\n";
        });
    }
    
    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ready.store(true);
        ready.notify_one();  // Wake only one waiter
    });
    
    for (auto& t : waiters) {
        t.join();
    }
    notifier.join();
    
    return 0;
}
```

### atomic::notify_all

```cpp
#include <atomic>
#include <thread>
#include <vector>

int main() {
    std::atomic<bool> ready(false);
    
    std::vector<std::thread> waiters;
    for (int i = 0; i < 5; ++i) {
        waiters.emplace_back([&ready, i]() {
            ready.wait(false);
            std::cout << "Waiter " << i << " woke\n";
        });
    }
    
    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ready.store(true);
        ready.notify_all();  // Wake all waiters
    });
    
    for (auto& t : waiters) {
        t.join();
    }
    notifier.join();
    
    return 0;
}
```

### atomic::wait_for with Timeout

```cpp
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>

int main() {
    std::atomic<int> value(0);
    
    std::thread waiter([&]() {
        auto start = std::chrono::steady_clock::now();
        
        if (value.wait_for(0, std::chrono::seconds(2)) == std::atomic_status::timeout) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            std::cout << "Timeout after " 
                      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
                      << "ms\n";
        } else {
            std::cout << "Value changed\n";
        }
    });
    
    std::thread notifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        value.store(42);
        value.notify_one();
    });
    
    waiter.join();
    notifier.join();
    
    return 0;
}
```

### Realistic Example: Producer-Consumer with atomic

```cpp
#include <atomic>
#include <thread>
#include <queue>
#include <iostream>

template<typename T>
class AtomicQueue {
private:
    std::queue<T> queue_;
    std::atomic<bool> has_data_{false};
    std::mutex mtx_;
    
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(value));
        }
        has_data_.store(true, std::memory_order_release);
        has_data_.notify_one();
    }
    
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    void wait_and_pop(T& value) {
        has_data_.wait(false);
        std::lock_guard<std::mutex> lock(mtx_);
        value = std::move(queue_.front());
        queue_.pop();
        if (queue_.empty()) {
            has_data_.store(false, std::memory_order_release);
        }
    }
};

int main() {
    AtomicQueue<int> queue;
    
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            queue.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    std::thread consumer([&]() {
        for (int i = 0; i < 10; ++i) {
            int value;
            queue.wait_and_pop(value);
            std::cout << "Consumed: " << value << "\n";
        }
    });
    
    producer.join();
    consumer.join();
    
    return 0;
}
```

## Underlying Implementation

### atomic::wait Implementation

```cpp
namespace std {
    template<typename T>
    void atomic<T>::wait(T old, std::memory_order order) const noexcept {
        // Check if value has already changed
        T current = load(order);
        if (current != old) return;
        
        // Wait for notification
        futex_wait(&value_, old);
    }
    
    template<typename T>
    template<typename Predicate>
    void atomic<T>::wait(T old, Predicate pred, std::memory_order order) const noexcept {
        while (!pred(load(order))) {
            futex_wait(&value_, load(order));
        }
    }
}
```

### Futex-Based Implementation (Linux)

```cpp
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

class FutexWaiter {
private:
    static int futex_wait(int* addr, int expected) {
        return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
    }
    
    static int futex_wake(int* addr, int count) {
        return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
    }
    
public:
    static void wait(std::atomic<int>* atomic, int expected) {
        int* addr = reinterpret_cast<int*>(atomic);
        while (atomic->load() == expected) {
            futex_wait(addr, expected);
        }
    }
    
    static void notify_one(std::atomic<int>* atomic) {
        int* addr = reinterpret_cast<int*>(atomic);
        futex_wake(addr, 1);
    }
    
    static void notify_all(std::atomic<int>* atomic) {
        int* addr = reinterpret_cast<int*>(atomic);
        futex_wake(addr, INT_MAX);
    }
};
```

### Windows Implementation

```cpp
#include <windows.h>

class WindowsWaiter {
public:
    static void wait(std::atomic<int>* atomic, int expected) {
        int* addr = reinterpret_cast<int*>(atomic);
        while (atomic->load() == expected) {
            WaitOnAddress(addr, &expected, sizeof(int), INFINITE);
        }
    }
    
    static void notify_one(std::atomic<int>* atomic) {
        int* addr = reinterpret_cast<int*>(atomic);
        WakeByAddressSingle(addr);
    }
    
    static void notify_all(std::atomic<int>* atomic) {
        int* addr = reinterpret_cast<int*>(atomic);
        WakeByAddressAll(addr);
    }
};
```

### Fallback Implementation (No OS Support)

```cpp
class FallbackWaiter {
private:
    static std::mutex global_mtx;
    static std::condition_variable global_cv;
    static std::unordered_map<void*, std::vector<std::condition_variable*>> waiters;
    
public:
    static void wait(std::atomic<int>* atomic, int expected) {
        std::unique_lock<std::mutex> lock(global_mtx);
        std::condition_variable cv;
        waiters[atomic].push_back(&cv);
        
        while (atomic->load() == expected) {
            cv.wait(lock);
        }
        
        // Remove from waiters
        auto& vec = waiters[atomic];
        vec.erase(std::remove(vec.begin(), vec.end(), &cv), vec.end());
    }
    
    static void notify_one(std::atomic<int>* atomic) {
        std::lock_guard<std::mutex> lock(global_mtx);
        auto it = waiters.find(atomic);
        if (it != waiters.end() && !it->second.empty()) {
            it->second.front()->notify_one();
        }
    }
    
    static void notify_all(std::atomic<int>* atomic) {
        std::lock_guard<std::mutex> lock(global_mtx);
        auto it = waiters.find(atomic);
        if (it != waiters.end()) {
            for (auto* cv : it->second) {
                cv->notify_all();
            }
        }
    }
};
```

### atomic::wait_for Implementation

```cpp
namespace std {
    enum class atomic_status { ready, timeout };
    
    template<typename T>
    template<typename Rep, typename Period>
    atomic_status atomic<T>::wait_for(T old, 
                                      const chrono::duration<Rep, Period>& rel_time,
                                      std::memory_order order) const noexcept {
        auto deadline = chrono::steady_clock::now() + rel_time;
        
        while (load(order) == old) {
            if (chrono::steady_clock::now() >= deadline) {
                return atomic_status::timeout;
            }
            
            // Wait with shorter timeout
            auto remaining = deadline - chrono::steady_clock::now();
            futex_wait_timeout(&value_, old, remaining);
        }
        
        return atomic_status::ready;
    }
}
```

### Spurious Wakeup Handling

```cpp
// atomic::wait must handle spurious wakeups
void robust_wait(std::atomic<int>& atomic, int expected) {
    while (atomic.load() == expected) {
        atomic.wait(expected);  // May wake spuriously
        // Re-check condition in loop
    }
}

// Or use predicate version
void robust_wait_with_predicate(std::atomic<int>& atomic) {
    atomic.wait(0, [](int v) { return v != 0; });
}
```

### Memory Ordering Considerations

```cpp
// notify provides release semantics
void atomic<T>::notify_one() noexcept {
    std::atomic_thread_fence(std::memory_order_release);
    // Wake waiters
}

// wait provides acquire semantics
void atomic<T>::wait(T old, std::memory_order order) const noexcept {
    // Wait for notification
    // On wake, acquire fence ensures visibility
    std::atomic_thread_fence(std::memory_order_acquire);
}
```

### Comparison with Condition Variables

```cpp
// Condition variable approach
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void waiter() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
}

void notifier() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();
}

// atomic wait approach (no mutex needed)
std::atomic<bool> ready(false);

void waiter() {
    ready.wait(false);
}

void notifier() {
    ready.store(true);
    ready.notify_one();
}
```

### Performance Characteristics

```cpp
// atomic::wait is typically more efficient than condition variables
// - No mutex overhead
// - Direct futex system call
// - Lower memory footprint

// But has limitations:
// - Only works with atomic types
// - No predicate support (must use loop or wait_for)
// - Platform-specific implementation
```

## Best Practices

1. Use atomic::wait when you don't need a mutex
2. Always use predicate version or loop to handle spurious wakeups
3. Use notify_one for single consumer, notify_all for multiple
4. Be aware of platform-specific implementations
5. Consider fallback for platforms without futex support
6. Use wait_for with timeout to avoid indefinite blocking
7. Ensure proper memory ordering around wait/notify
8. Compare with condition variables for complex synchronization
9. Test on target platforms (behavior may vary)
10. Document wait conditions clearly
