# Condition Variables: Usage and Implementation

## Practical Usage

### Basic Condition Variable Usage

```cpp
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void worker() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    std::cout << "Worker proceeding\n";
}

int main() {
    std::thread t(worker);
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();
    
    t.join();
    return 0;
}
```

### Producer-Consumer Pattern

```cpp
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <iostream>

std::mutex mtx;
std::condition_variable cv;
std::queue<int> queue;
const size_t max_size = 10;

void producer(int id) {
    for (int i = 0; i < 10; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return queue.size() < max_size; });
        
        queue.push(i);
        std::cout << "Producer " << id << " produced " << i << "\n";
        
        lock.unlock();
        cv.notify_one();
    }
}

void consumer(int id) {
    for (int i = 0; i < 10; ++i) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { return !queue.empty(); });
        
        int value = queue.front();
        queue.pop();
        std::cout << "Consumer " << id << " consumed " << value << "\n";
        
        lock.unlock();
        cv.notify_one();
    }
}

int main() {
    std::thread p1(producer, 1);
    std::thread c1(consumer, 1);
    
    p1.join();
    c1.join();
    
    return 0;
}
```

### std::condition_variable_any - With Any Lockable

```cpp
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>

std::shared_mutex shared_mtx;
std::condition_variable_any cv_any;
int shared_data = 0;

void reader() {
    std::shared_lock<std::shared_mutex> lock(shared_mtx);
    cv_any.wait(lock, [] { return shared_data > 0; });
    // Read data
}

void writer() {
    std::unique_lock<std::shared_mutex> lock(shared_mtx);
    shared_data = 42;
    cv_any.notify_all();
}
```

### Timed Wait

```cpp
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void timed_wait_example() {
    std::unique_lock<std::mutex> lock(mtx);
    
    if (cv.wait_for(lock, std::chrono::seconds(1), [] { return ready; })) {
        std::cout << "Condition met\n";
    } else {
        std::cout << "Timeout\n";
    }
}

int main() {
    std::thread t(timed_wait_example);
    t.join();
    return 0;
}
```

### Realistic Example: Thread Pool with Work Queue

```cpp
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <vector>

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ThreadPool(size_t threads) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }
    
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

## Underlying Implementation

### OS-Level Primitives

Condition variables are implemented using OS synchronization primitives:

- **Linux/macOS**: pthread condition variables (pthread_cond_t)
- **Windows**: CONDITION_VARIABLE (Vista+)
- **Other**: Platform-specific primitives

### std::condition_variable Implementation

```cpp
// Simplified conceptual implementation
namespace std {
    class condition_variable {
    public:
        condition_variable();
        ~condition_variable();
        
        condition_variable(const condition_variable&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;
        
        void notify_one() noexcept;
        void notify_all() noexcept;
        
        template<class Predicate>
        void wait(std::unique_lock<std::mutex>& lock, Predicate pred) {
            while (!pred()) {
                native_wait(lock);
            }
        }
        
        template<class Rep, class Period, class Predicate>
        bool wait_for(std::unique_lock<std::mutex>& lock,
                      const std::chrono::duration<Rep, Period>& rel_time,
                      Predicate pred) {
            return wait_until(lock, 
                             std::chrono::steady_clock::now() + rel_time,
                             pred);
        }
        
    private:
        native_handle_type native_handle_;
    };
}
```

### POSIX Condition Variable Implementation (Linux/macOS)

```cpp
#include <pthread.h>

class condition_variable {
private:
    pthread_cond_t native_handle_;
    
public:
    condition_variable() {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);  // Avoid system time changes
        pthread_cond_init(&native_handle_, &attr);
        pthread_condattr_destroy(&attr);
    }
    
    ~condition_variable() {
        pthread_cond_destroy(&native_handle_);
    }
    
    void notify_one() noexcept {
        pthread_cond_signal(&native_handle_);
    }
    
    void notify_all() noexcept {
        pthread_cond_broadcast(&native_handle_);
    }
    
    void wait(std::unique_lock<std::mutex>& lock) {
        // Unlock mutex and wait atomically
        pthread_cond_wait(&native_handle_, 
                         lock.mutex()->native_handle());
        // Mutex is re-acquired when returning
    }
    
    bool wait_until(std::unique_lock<std::mutex>& lock,
                   const std::chrono::steady_clock::time_point& timeout) {
        struct timespec ts;
        ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(
            timeout.time_since_epoch()).count();
        ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(
            timeout.time_since_epoch() % std::chrono::seconds(1)).count();
        
        int result = pthread_cond_timedwait(&native_handle_,
                                           lock.mutex()->native_handle(),
                                           &ts);
        return result != ETIMEDOUT;
    }
};
```

### Win32 Condition Variable Implementation (Windows)

```cpp
#include <windows.h>

class condition_variable {
private:
    CONDITION_VARIABLE native_handle_;
    
public:
    condition_variable() {
        InitializeConditionVariable(&native_handle_);
    }
    
    ~condition_variable() {
        // No cleanup needed for CONDITION_VARIABLE
    }
    
    void notify_one() noexcept {
        WakeConditionVariable(&native_handle_);
    }
    
    void notify_all() noexcept {
        WakeAllConditionVariable(&native_handle_);
    }
    
    void wait(std::unique_lock<std::mutex>& lock) {
        SleepConditionVariableCS(&native_handle_,
                               lock.mutex()->native_handle(),
                               INFINITE);
    }
    
    bool wait_for(std::unique_lock<std::mutex>& lock,
                  const std::chrono::milliseconds& timeout) {
        return SleepConditionVariableCS(&native_handle_,
                                      lock.mutex()->native_handle(),
                                      timeout.count()) != 0;
    }
};
```

### std::condition_variable_any Implementation

```cpp
// Simplified implementation using generic lockable
class condition_variable_any {
private:
    std::mutex internal_mtx_;
    std::condition_variable internal_cv_;
    
public:
    template<class Lock, class Predicate>
    void wait(Lock& lock, Predicate pred) {
        std::unique_lock<std::mutex> internal_lock(internal_mtx_);
        
        // Unlock the user's lock and wait
        lock.unlock();
        internal_cv.wait(internal_lock, pred);
        
        // Re-acquire the user's lock
        lock.lock();
    }
    
    void notify_one() noexcept {
        std::lock_guard<std::mutex> lock(internal_mtx_);
        internal_cv.notify_one();
    }
    
    void notify_all() noexcept {
        std::lock_guard<std::mutex> lock(internal_mtx_);
        internal_cv.notify_all();
    }
};
```

### Spurious Wakeups

Condition variables can experience spurious wakeups:

```cpp
// WRONG: Doesn't handle spurious wakeups
void bad_wait() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);  // Might wake up even if condition not met
    // Proceed assuming condition is met
}

// CORRECT: Uses predicate to handle spurious wakeups
void good_wait() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return condition_met; });  // Re-checks condition
    // Proceed only when condition is actually met
}
```

### Implementation of Predicate-Based Wait

```cpp
template<class Predicate>
void wait(std::unique_lock<std::mutex>& lock, Predicate pred) {
    while (!pred()) {
        // Unlock and wait
        native_wait(lock);
        // On wake, mutex is re-acquired
        // Loop checks predicate again
    }
}
```

### Memory Ordering and Condition Variables

Condition variables provide acquire-release semantics:

```cpp
// notify_one() provides release semantics
void notify_one() noexcept {
    std::atomic_thread_fence(std::memory_order_release);
    native_notify_one();
}

// wait() provides acquire semantics
void wait(std::unique_lock<std::mutex>& lock) {
    native_wait(lock);
    std::atomic_thread_fence(std::memory_order_acquire);
}
```

### Lost Wakeup Problem

Proper synchronization prevents lost wakeups:

```cpp
// WRONG: Lost wakeup possible
void producer_wrong() {
    data = 42;
    cv.notify_one();  // Consumer might not be waiting yet
}

void consumer_wrong() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock);  // Might miss the notification
}

// CORRECT: Always hold mutex when modifying condition
void producer_correct() {
    std::lock_guard<std::mutex> lock(mtx);
    data = 42;
    cv.notify_one();  // Consumer will see the change
}

void consumer_correct() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return data != 0; });  // Checks condition
}
```

### Broadcast vs Signal

```cpp
// notify_one() wakes one waiting thread
void single_notifier() {
    std::lock_guard<std::mutex> lock(mtx);
    condition = true;
    cv.notify_one();  // Only one thread wakes
}

// notify_all() wakes all waiting threads
void broadcast_notifier() {
    std::lock_guard<std::mutex> lock(mtx);
    condition = true;
    cv.notify_all();  // All waiting threads wake
}
```

### Implementation Details

The typical implementation uses:

1. **Wait queue**: A queue of waiting threads
2. **Mutex protection**: Internal mutex protects the wait queue
3. **Atomic operations**: For efficient signaling
4. **Futex (Linux)**: Fast userspace mutex for efficient waiting

```cpp
// Conceptual futex-based implementation (Linux)
void wait(std::unique_lock<std::mutex>& lock) {
    // Add current thread to wait queue
    add_to_wait_queue();
    
    // Unlock the mutex
    lock.unlock();
    
    // Futex wait (efficient kernel wait)
    futex_wait(&futex_word_, FUTEX_WAIT);
    
    // Re-acquire mutex when woken
    lock.lock();
    
    // Remove from wait queue
    remove_from_wait_queue();
}
```

## Best Practices

1. Always use a predicate with wait() to handle spurious wakeups
2. Always hold the mutex when modifying the condition
3. Always hold the mutex when calling notify_one() or notify_all()
4. Use notify_one() for single-consumer scenarios
5. Use notify_all() for multiple consumers or state changes
6. Keep critical sections as small as possible
7. Be aware of the lost wakeup problem
8. Use condition_variable_any only when you need non-standard mutex types
9. Prefer steady_clock for timeouts to avoid system time changes
10. Ensure the predicate checks are thread-safe
