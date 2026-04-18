# C++20 Synchronization Primitives: Usage and Implementation

## Practical Usage

### std::latch - One-Time Barrier

```cpp
#include <latch>
#include <thread>
#include <vector>
#include <iostream>

void worker(std::latch& done, int id) {
    std::cout << "Worker " << id << " starting\n";
    // Do work
    done.count_down();  // Signal completion
    std::cout << "Worker " << id << " done\n";
}

int main() {
    const int num_workers = 4;
    std::latch done(num_workers);
    
    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back(worker, std::ref(done), i);
    }
    
    // Wait for all workers to finish
    done.wait();
    std::cout << "All workers completed\n";
    
    for (auto& t : workers) {
        t.join();
    }
    
    return 0;
}
```

### std::barrier - Reusable Barrier

```cpp
#include <barrier>
#include <thread>
#include <vector>
#include <iostream>

void phase_worker(std::barrier<>& sync, int id) {
    for (int phase = 0; phase < 3; ++phase) {
        std::cout << "Thread " << id << " phase " << phase << "\n";
        sync.arrive_and_wait();  // Wait for all threads
        std::cout << "Thread " << id << " past barrier\n";
    }
}

int main() {
    const int num_threads = 3;
    std::barrier sync(num_threads, []() noexcept {
        std::cout << "--- Phase complete ---\n";
    });
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(phase_worker, std::ref(sync), i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### std::semaphore - Counting Semaphore

```cpp
#include <semaphore>
#include <thread>
#include <vector>
#include <iostream>

std::counting_semaphore<3> sem(3);  // Max 3 concurrent accesses

void access_resource(int id) {
    sem.acquire();  // Wait for available slot
    std::cout << "Thread " << id << " accessing resource\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Thread " << id << " releasing resource\n";
    sem.release();  // Release slot
}

int main() {
    const int num_threads = 6;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(access_resource, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### Binary Semaphore (Mutex-like)

```cpp
#include <semaphore>
#include <thread>
#include <iostream>

std::binary_semaphore sem(1);  // Binary semaphore (1 or 0)

void critical_section(int id) {
    sem.acquire();
    std::cout << "Thread " << id << " in critical section\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Thread " << id << " leaving critical section\n";
    sem.release();
}

int main() {
    std::thread t1(critical_section, 1);
    std::thread t2(critical_section, 2);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

### Semaphore with Timeout

```cpp
#include <semaphore>
#include <thread>
#include <chrono>
#include <iostream>

std::counting_semaphore<1> sem(0);

void producer() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    sem.release();
    std::cout << "Producer released semaphore\n";
}

void consumer() {
    std::cout << "Consumer waiting...\n";
    
    if (sem.try_acquire_for(std::chrono::seconds(1))) {
        std::cout << "Consumer acquired semaphore\n";
    } else {
        std::cout << "Consumer timeout\n";
    }
}

int main() {
    std::thread p(producer);
    std::thread c(consumer);
    
    p.join();
    c.join();
    
    return 0;
}
```

### Realistic Example: Thread Pool with Semaphore

```cpp
#include <semaphore>
#include <thread>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::counting_semaphore<> task_available_{0};
    std::counting_semaphore<> task_slots_{100};  // Limit queue size
    std::mutex mtx_;
    bool stop_;
    
public:
    ThreadPool(size_t threads) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    task_available_.acquire();
                    
                    std::function<void()> task;
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task_slots_.release();
                    task();
                }
            });
        }
    }
    
    template<class F>
    bool enqueue(F&& f) {
        if (!task_slots_.try_acquire_for(std::chrono::seconds(1))) {
            return false;  // Queue full
        }
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) return false;
            tasks_.emplace(std::forward<F>(f));
        }
        
        task_available_.release();
        return true;
    }
    
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        
        for (size_t i = 0; i < workers_.size(); ++i) {
            task_available_.release();
        }
        
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

## Underlying Implementation

### std::latch Implementation

```cpp
#include <atomic>
#include <condition_variable>

class latch {
private:
    std::atomic<std::ptrdiff_t> counter_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
public:
    explicit latch(std::ptrdiff_t expected) 
        : counter_(expected) {}
    
    ~latch() = default;
    
    void count_down(std::ptrdiff_t n = 1) {
        std::ptrdiff_t old = counter_.fetch_sub(n, std::memory_order_acq_rel);
        if (old == n) {
            // Counter reached zero
            std::lock_guard<std::mutex> lock(mtx_);
            cv_.notify_all();
        }
    }
    
    bool try_wait() const noexcept {
        return counter_.load(std::memory_order_acquire) == 0;
    }
    
    void wait() const {
        if (try_wait()) return;
        
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return try_wait(); });
    }
    
    void arrive_and_wait(std::ptrdiff_t n = 1) {
        count_down(n);
        wait();
    }
};
```

### std::barrier Implementation

```cpp
#include <atomic>
#include <vector>
#include <condition_variable>

template<typename CompletionFunction = void(*)() noexcept>
class barrier {
private:
    struct completion_wrapper {
        CompletionFunction completion_;
        
        void operator()() noexcept(noexcept(completion_())) {
            if constexpr (!std::is_same_v<CompletionFunction, void(*)() noexcept>) {
                completion_();
            }
        }
    };
    
    std::atomic<std::ptrdiff_t> counter_;
    std::ptrdiff_t expected_;
    completion_wrapper completion_;
    std::atomic<std::ptrdiff_t> phase_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
public:
    explicit barrier(std::ptrdiff_t expected, 
                     CompletionFunction f = []() noexcept {})
        : counter_(expected), expected_(expected), 
          completion_(f), phase_(0) {}
    
    ~barrier() = default;
    
    void arrive_and_wait() {
        std::ptrdiff_t old_phase = phase_.load(std::memory_order_acquire);
        std::ptrdiff_t new_phase = old_phase + 1;
        
        std::ptrdiff_t arrived = counter_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        
        if (arrived == 0) {
            // Last thread to arrive
            counter_.store(expected_, std::memory_order_release);
            completion_();
            phase_.store(new_phase, std::memory_order_release);
            cv_.notify_all();
        } else {
            // Wait for other threads
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this, old_phase] { 
                return phase_.load(std::memory_order_acquire) != old_phase;
            });
        }
    }
    
    void arrive(std::ptrdiff_t n = 1) {
        std::ptrdiff_t arrived = counter_.fetch_sub(n, std::memory_order_acq_rel) - n;
        
        if (arrived == 0) {
            counter_.store(expected_, std::memory_order_release);
            completion_();
            phase_.fetch_add(1, std::memory_order_release);
            cv_.notify_all();
        }
    }
};
```

### std::counting_semaphore Implementation

```cpp
#include <atomic>
#include <condition_variable>

template<std::ptrdiff_t LeastMaxValue = std::numeric_limits<std::ptrdiff_t>::max()>
class counting_semaphore {
private:
    std::atomic<std::ptrdiff_t> counter_;
    std::condition_variable cv_;
    std::mutex mtx_;
    
public:
    explicit counting_semaphore(std::ptrdiff_t desired) 
        : counter_(desired) {}
    
    ~counting_semaphore() = default;
    
    void release(std::ptrdiff_t update = 1) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            counter_.fetch_add(update, std::memory_order_release);
        }
        cv_.notify_all();
    }
    
    void acquire() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { 
            return counter_.load(std::memory_order_acquire) > 0;
        });
        counter_.fetch_sub(1, std::memory_order_acq_rel);
    }
    
    bool try_acquire() {
        std::ptrdiff_t expected = counter_.load(std::memory_order_acquire);
        if (expected == 0) return false;
        
        return counter_.compare_exchange_strong(
            expected, expected - 1,
            std::memory_order_acquire,
            std::memory_order_acquire);
    }
    
    template<class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& rel_time) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        if (counter_.load(std::memory_order_acquire) > 0) {
            counter_.fetch_sub(1, std::memory_order_acq_rel);
            return true;
        }
        
        return cv_.wait_for(lock, rel_time, [this] {
            if (counter_.load(std::memory_order_acquire) > 0) {
                counter_.fetch_sub(1, std::memory_order_acq_rel);
                return true;
            }
            return false;
        });
    }
};
```

### std::binary_semaphore Implementation

```cpp
// Binary semaphore is a specialization of counting_semaphore
using binary_semaphore = counting_semaphore<1>;
```

### Futex-Based Implementation (Linux)

```cpp
// Linux futex-based implementation for efficiency
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

class futex_semaphore {
private:
    std::atomic<int> counter_;
    
    static int futex_wait(std::atomic<int>* addr, int expected) {
        return syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0);
    }
    
    static int futex_wake(std::atomic<int>* addr, int count) {
        return syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
    }
    
public:
    explicit futex_semaphore(int initial) : counter_(initial) {}
    
    void release() {
        counter_.fetch_add(1, std::memory_order_release);
        futex_wake(&counter_, 1);
    }
    
    void acquire() {
        int expected = counter_.load(std::memory_order_acquire);
        while (expected == 0 || 
               !counter_.compare_exchange_weak(
                   expected, expected - 1,
                   std::memory_order_acquire,
                   std::memory_order_acquire)) {
            if (expected == 0) {
                futex_wait(&counter_, 0);
                expected = counter_.load(std::memory_order_acquire);
            }
        }
    }
};
```

### Windows Semaphore Implementation

```cpp
#include <windows.h>

class windows_semaphore {
private:
    HANDLE handle_;
    
public:
    explicit windows_semaphore(int initial, int max) {
        handle_ = CreateSemaphore(nullptr, initial, max, nullptr);
    }
    
    ~windows_semaphore() {
        CloseHandle(handle_);
    }
    
    void release(int count = 1) {
        ReleaseSemaphore(handle_, count, nullptr);
    }
    
    void acquire() {
        WaitForSingleObject(handle_, INFINITE);
    }
    
    bool try_acquire_for(std::chrono::milliseconds timeout) {
        return WaitForSingleObject(handle_, timeout.count()) == WAIT_OBJECT_0;
    }
};
```

### Memory Ordering Considerations

```cpp
// Release semantics for counter updates
void release() {
    // Ensure all writes before release are visible
    std::atomic_thread_fence(std::memory_order_release);
    counter_.fetch_add(1, std::memory_order_relaxed);
    notify_waiters();
}

// Acquire semantics for counter reads
void acquire() {
    wait_for_counter_positive();
    // Ensure all writes after acquire see previous releases
    std::atomic_thread_fence(std::memory_order_acquire);
    counter_.fetch_sub(1, std::memory_order_relaxed);
}
```

### Spurious Wakeup Handling

```cpp
// Semaphores must handle spurious wakeups
void robust_acquire() {
    std::unique_lock<std::mutex> lock(mtx_);
    
    while (counter_.load(std::memory_order_acquire) == 0) {
        cv_.wait(lock);  // May wake spuriously
    }
    
    counter_.fetch_sub(1, std::memory_order_acq_rel);
}
```

### Completion Function in Barrier

```cpp
// The completion function runs after each phase
std::barrier sync(4, []() noexcept {
    // This runs exactly once per phase
    // Useful for phase-specific cleanup or coordination
    std::cout << "All threads completed phase\n";
});
```

### Latch vs Barrier Comparison

```cpp
// Latch: One-time use, cannot be reused
std::latch done(4);
done.wait();  // Blocks until count reaches 0
// done is now "spent" - cannot be reused

// Barrier: Reusable, multiple phases
std::barrier sync(4);
sync.arrive_and_wait();  // Phase 1
sync.arrive_and_wait();  // Phase 2
sync.arrive_and_wait();  // Phase 3
// Can be used indefinitely
```

## Best Practices

1. Use latch for one-time synchronization (e.g., initialization)
2. Use barrier for phased algorithms (e.g., iterative computations)
3. Use semaphore for resource pooling (e.g., connection limits)
4. Prefer counting_semaphore over manual condition variables
5. Be aware that latch cannot be reused
6. Barrier completion functions should be noexcept
7. Use try_acquire with timeout to avoid deadlocks
8. Binary semaphore can replace mutex for simple cases
9. Consider fairness when choosing between primitives
10. Test on target platforms (implementation may vary)
