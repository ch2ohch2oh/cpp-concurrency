# Mutex Types: Usage and Implementation

## Practical Usage

### std::mutex - Basic Mutex

The `std::mutex` is the simplest synchronization primitive in C++. It provides exclusive ownership semantics - only one thread can hold the lock at any given time. This is the foundation for most thread-safe operations.

**Key characteristics:**
- Non-copyable and non-movable
- Must be locked before accessing shared data
- Use RAII wrappers like `lock_guard` to ensure proper unlocking
- If a thread tries to lock a mutex it already owns, behavior is undefined (deadlock)

**When to use:**
- Basic thread synchronization
- Protecting small critical sections
- Simple exclusive access scenarios

```cpp
#include <mutex>
#include <thread>
#include <iostream>

std::mutex mtx;
int counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);
    ++counter;
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter << "\n";  // Output: 2
    return 0;
}
```

### std::recursive_mutex - Reentrant Mutex

A recursive mutex allows the same thread to lock the mutex multiple times without causing a deadlock. This is useful when a function that acquires a lock might call itself or another function that also needs the same lock.

**Key characteristics:**
- Tracks lock count per thread
- Same thread can lock multiple times (reentrant)
- Different threads block like a regular mutex (exclusive access)
- Must unlock the same number of times as locked
- Slightly higher overhead than `std::mutex`

**When to use:**
- Recursive functions that need synchronization
- When you can't easily restructure code to avoid nested locks
- Legacy code with complex call patterns

**Warning:** Recursive mutexes can hide design issues. Consider refactoring to avoid nested locks when possible.

```cpp
#include <mutex>
#include <thread>
#include <iostream>
#include <vector>

// Example: A class with nested method calls that need the same lock
class DataProcessor {
private:
    std::recursive_mutex mtx_;
    std::vector<int> data_;
    
public:
    void add_value(int value) {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        data_.push_back(value);
        
        // Calls another method that also locks the same mutex
        // This would deadlock with std::mutex!
        if (value > 100) {
            process_large_value(value);
        }
    }
    
    void process_large_value(int value) {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        // Safe to lock again because we already own this mutex
        std::cout << "Processing large value: " << value << "\n";
        data_.push_back(value / 2);
    }
    
    void batch_add(const std::vector<int>& values) {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        for (int v : values) {
            // Calls add_value which also locks
            // Safe with recursive_mutex, deadlock with std::mutex
            add_value(v);
        }
    }
    
    size_t size() const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        return data_.size();
    }
};

int main() {
    DataProcessor processor;
    
    // Multiple threads calling methods that nest locks
    std::thread t1([&]() {
        processor.add_value(50);
        processor.add_value(150);  // Triggers nested lock
    });
    
    std::thread t2([&]() {
        std::vector<int> batch = {10, 200, 30};
        processor.batch_add(batch);  // Triggers nested locks
    });
    
    t1.join();
    t2.join();
    
    std::cout << "Total size: " << processor.size() << "\n";
    return 0;
}
```

**Note:** If you replace `std::recursive_mutex` with `std::mutex` in this example, the program will deadlock when `add_value()` calls `process_large_value()` or when `batch_add()` calls `add_value()`.

### std::timed_mutex - Timeout Support

The `std::timed_mutex` extends the basic mutex with timeout capabilities. It allows you to attempt to acquire a lock for a specified duration, enabling more robust error handling and avoiding indefinite blocking.

**Key characteristics:**
- Supports `try_lock_for()` - try to lock for a specified duration (e.g., 100ms max wait; returns immediately if lock is available)
- Supports `try_lock_until()` - try to lock until a specific time point
- Returns `false` if lock cannot be acquired within timeout
- Useful for implementing responsive applications
- Can prevent deadlocks by timing out

**When to use:**
- When you need bounded waiting times
- Implementing timeout-based retry logic
- Building responsive user interfaces
- Preventing indefinite blocking in distributed systems

```cpp
#include <mutex>
#include <thread>
#include <chrono>
#include <iostream>

std::timed_mutex timed_mtx;

void try_lock_with_timeout() {
    if (timed_mtx.try_lock_for(std::chrono::milliseconds(100))) {
        std::lock_guard<std::timed_mutex> lock(timed_mtx, std::adopt_lock);
        std::cout << "Lock acquired\n";
    } else {
        std::cout << "Failed to acquire lock\n";
    }
}

int main() {
    std::thread t(try_lock_with_timeout);
    t.join();
    return 0;
}
```

### std::shared_mutex - Reader-Writer Lock (C++17)

The `std::shared_mutex` implements a reader-writer lock pattern, allowing multiple readers to access shared data simultaneously while ensuring exclusive access for writers. This is ideal for read-heavy workloads.

**Key characteristics:**
- Multiple threads can hold shared (read) locks simultaneously
- Only one thread can hold exclusive (write) lock at a time
- Writers block both readers and other writers
- Readers block writers but not other readers
- Uses `shared_lock` for reading and `unique_lock` for writing

**When to use:**
- Data structures that are read much more often than written
- Caches, configuration data, reference data
- Scenarios where read operations dominate

**Performance benefit:** Significantly reduces contention when multiple threads need to read the same data concurrently.

```cpp
#include <shared_mutex>
#include <thread>
#include <iostream>

std::shared_mutex shared_mtx;
int shared_data = 0;

void reader() {
    std::shared_lock<std::shared_mutex> lock(shared_mtx);
    std::cout << "Read: " << shared_data << "\n";
}

void writer(int value) {
    std::unique_lock<std::shared_mutex> lock(shared_mtx);
    shared_data = value;
    std::cout << "Written: " << value << "\n";
}

int main() {
    std::thread r1(reader);
    std::thread r2(reader);
    std::thread w1(writer, 42);
    
    r1.join();
    r2.join();
    w1.join();
    
    return 0;
}
```

### Realistic Example: Thread-Safe Queue

This example demonstrates a complete thread-safe queue implementation combining mutex protection with condition variables for efficient waiting. This pattern is commonly used in producer-consumer scenarios.

**Key components:**
- `std::mutex`: Protects the internal queue
- `std::condition_variable`: Enables efficient waiting for data
- `push()`: Adds data and notifies one waiting thread (`notify_one()` wakes exactly one thread; `notify_all()` would wake all waiting threads)
- `try_pop()`: Non-blocking attempt to retrieve data
- `wait_and_pop()`: Blocks until data is available

**Design decisions:**
- `lock_guard` for simple scoped locking (push, try_pop)
- `unique_lock` for condition variable operations (wait_and_pop)
- Predicate in `cv.wait()` prevents spurious wakeups - condition variables can wake up without being notified; the predicate re-checks the condition to ensure it's actually true
- `mutable` mutex allows const methods to lock - const methods need to modify the mutex state (lock/unlock) even though they don't modify the logical data

```cpp
#include <mutex>
#include <queue>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
    
public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(value);
        cv_.notify_one();
    }
    
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        value = queue_.front();
        queue_.pop();
        return true;
    }
    
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }
};
```

## Underlying Implementation

### OS-Level Primitives

Mutexes are implemented using OS synchronization primitives:

- **Linux/macOS**: pthread mutexes (pthread_mutex_t)
- **Windows**: CRITICAL_SECTION or SRWLOCK
- **Other**: Platform-specific primitives

### std::mutex Implementation

The standard library mutex is a thin wrapper around OS-level synchronization primitives. This abstraction allows the same C++ code to work across different platforms while leveraging optimal native implementations.

**Key implementation details:**
- Deleted copy/move constructors ensure mutex cannot be copied or moved
- `native_handle()` provides access to the underlying OS primitive for advanced use
- `constexpr` constructor enables static initialization (mutex can be initialized at compile-time for global/static variables)
- `noexcept` guarantees the constructor won't throw exceptions
- Destructor is not defaulted because it must call OS cleanup functions (e.g., pthread_mutex_destroy)
- All operations are noexcept where possible

**Why this matters:**
- Understanding the implementation helps with debugging
- Explains why mutexes have specific behaviors
- Shows the cost of abstraction (typically minimal)

```cpp
// Simplified conceptual implementation
namespace std {
    class mutex {
    public:
        constexpr mutex() noexcept = default;
        ~mutex();
        
        mutex(const mutex&) = delete;
        mutex& operator=(const mutex&) = delete;
        
        void lock() {
            // Call OS lock function
            native_lock(&native_handle_);
        }
        
        bool try_lock() noexcept {
            return native_try_lock(&native_handle_);
        }
        
        void unlock() noexcept {
            native_unlock(&native_handle_);
        }
        
        typedef void* native_handle_type;
        native_handle_type native_handle() { return &native_handle_; }
        
    private:
        native_handle_type native_handle_;
    };
}
```

### POSIX Mutex Implementation (Linux/macOS)

On Unix-like systems, C++ mutexes are typically implemented using POSIX threads (pthread). This provides a portable and well-tested synchronization mechanism.

**Key POSIX features used:**
- `pthread_mutexattr_t`: Configures mutex behavior
- `PTHREAD_MUTEX_DEFAULT`: Standard non-recursive mutex
- `pthread_mutex_init()`: Initializes the mutex with attributes
- `pthread_mutexattr_destroy()`: Cleans up attributes after use

**Implementation notes:**
- The mutex attribute is configured during construction
- Attributes are destroyed immediately after initialization to save resources
- Error handling is simplified in this example (real implementations check return values)

```cpp
#include <pthread.h>

class mutex {
private:
    pthread_mutex_t native_handle_;
    
public:
    mutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_DEFAULT);
        pthread_mutex_init(&native_handle_, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    
    ~mutex() {
        pthread_mutex_destroy(&native_handle_);
    }
    
    void lock() {
        pthread_mutex_lock(&native_handle_);
    }
    
    bool try_lock() noexcept {
        return pthread_mutex_trylock(&native_handle_) == 0;
    }
    
    void unlock() noexcept {
        pthread_mutex_unlock(&native_handle_);
    }
};
```

### Win32 Mutex Implementation (Windows)

Windows uses CRITICAL_SECTION for user-mode mutexes, which is faster than kernel mutexes for uncontended scenarios because it stays in user space.

**Key Windows API features:**
- `CRITICAL_SECTION`: Fast user-mode synchronization primitive
- `InitializeCriticalSection()`: Sets up the critical section
- `EnterCriticalSection()`: Acquires the lock (may enter kernel if contested)
- `TryEnterCriticalSection()`: Non-blocking lock attempt

**Performance characteristics:**
- Very fast when uncontended (no system call)
- Falls back to kernel synchronization when contested
- More efficient than kernel mutexes for typical use cases
- Cannot be used across process boundaries (unlike named mutexes)

```cpp
#include <windows.h>

class mutex {
private:
    CRITICAL_SECTION critical_section_;
    
public:
    mutex() {
        InitializeCriticalSection(&critical_section_);
    }
    
    ~mutex() {
        DeleteCriticalSection(&critical_section_);
    }
    
    void lock() {
        EnterCriticalSection(&critical_section_);
    }
    
    bool try_lock() noexcept {
        return TryEnterCriticalSection(&critical_section_) != 0;
    }
    
    void unlock() noexcept {
        LeaveCriticalSection(&critical_section_);
    }
};
```

### std::recursive_mutex Implementation

Recursive mutexes extend the basic mutex by tracking ownership and lock count. This allows the same thread to lock multiple times safely.

**Key implementation details:**
- `PTHREAD_MUTEX_RECURSIVE`: POSIX attribute for recursive behavior
- `owner_`: Tracks which thread currently owns the mutex
- `lock_count_`: Counts how many times the owning thread has locked
- Lock count increments on each lock by the owning thread
- Mutex is only released when lock count reaches zero

**Why track owner and count:**
- Prevents other threads from acquiring while one thread has nested locks
- Ensures proper release semantics
- Enables the recursive locking behavior

Recursive mutexes track the owning thread and lock count:

```cpp
class recursive_mutex {
private:
    pthread_mutex_t native_handle_;
    pthread_t owner_;
    size_t lock_count_;
    
public:
    recursive_mutex() : lock_count_(0), owner_(0) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&native_handle_, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    
    void lock() {
        pthread_mutex_lock(&native_handle_);
        
        if (owner_ != pthread_self()) {
            owner_ = pthread_self();
            lock_count_ = 1;
        } else {
            ++lock_count_;
        }
    }
    
    void unlock() {
        if (--lock_count_ == 0) {
            owner_ = 0;
        }
        pthread_mutex_unlock(&native_handle_);
    }
};
```

### std::timed_mutex Implementation

Timed mutexes combine mutex semantics with condition variables to implement timeout-based locking. This is more complex than a simple mutex but provides valuable functionality.

**Key implementation details:**
- Uses a condition variable (`pthread_cond_t`) for waiting with timeout
- `locked_` flag tracks the mutex state
- `pthread_cond_timedwait()`: Waits until a timeout or signal
- Absolute time calculation for timeout specification

**Timeout handling:**
- Converts relative timeout to absolute time
- Uses `CLOCK_REALTIME` for timeout calculation
- Returns `false` on timeout, `true` on success
- Properly cleans up lock on timeout

**Note:** This is a simplified implementation. Real implementations handle edge cases like spurious wakeups and signal interruptions.

Timed mutexes use timed locking primitives:

```cpp
class timed_mutex {
private:
    pthread_mutex_t native_handle_;
    pthread_cond_t cond_;
    bool locked_;
    
public:
    timed_mutex() : locked_(false) {
        pthread_mutex_init(&native_handle_, nullptr);
        pthread_cond_init(&cond_, nullptr);
    }
    
    ~timed_mutex() {
        pthread_mutex_destroy(&native_handle_);
        pthread_cond_destroy(&cond_);
    }
    
    bool try_lock_for(const std::chrono::milliseconds& timeout) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout.count() / 1000;
        ts.tv_nsec += (timeout.count() % 1000) * 1000000;
        
        pthread_mutex_lock(&native_handle_);
        
        while (locked_) {
            int result = pthread_cond_timedwait(&cond_, &native_handle_, &ts);
            if (result == ETIMEDOUT) {
                pthread_mutex_unlock(&native_handle_);
                return false;
            }
        }
        
        locked_ = true;
        pthread_mutex_unlock(&native_handle_);
        return true;
    }
};
```

### std::shared_mutex Implementation (C++17)

Shared mutexes implement reader-writer locking using condition variables to coordinate between readers and writers. This is a classic synchronization pattern.

**Key state variables:**
- `readers_`: Count of active readers
- `writer_`: Flag indicating if a writer has exclusive access
- `write_pending_`: Flag indicating a writer is waiting
- Two condition variables for reader and writer coordination

**Synchronization logic:**
- **Writers** wait until no readers are active and no other writer holds the lock
- **Readers** wait until no writer is active and no writer is pending
- This prevents writer starvation by blocking new readers when a writer is waiting
- Writers notify all readers when they release the lock
- Last reader notifies waiting writers

**Fairness considerations:**
- The `write_pending_` flag prevents writer starvation
- Without it, continuous reader traffic could block writers indefinitely
- This is a trade-off between throughput and fairness

Shared mutexes implement reader-writer locking:

```cpp
class shared_mutex {
private:
    std::mutex mtx_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
    int readers_;
    bool writer_;
    bool write_pending_;
    
public:
    shared_mutex() : readers_(0), writer_(false), write_pending_(false) {}
    
    void lock() {  // Exclusive lock (writer)
        std::unique_lock<std::mutex> lock(mtx_);
        write_pending_ = true;
        write_cv_.wait(lock, [this] { 
            return !writer_ && readers_ == 0; 
        });
        writer_ = true;
        write_pending_ = false;
    }
    
    void unlock() {  // Unlock exclusive
        std::lock_guard<std::mutex> lock(mtx_);
        writer_ = false;
        read_cv_.notify_all();
    }
    
    void lock_shared() {  // Shared lock (reader)
        std::unique_lock<std::mutex> lock(mtx_);
        read_cv_.wait(lock, [this] { 
            return !writer_ && !write_pending_; 
        });
        ++readers_;
    }
    
    void unlock_shared() {  // Unlock shared
        std::lock_guard<std::mutex> lock(mtx_);
        --readers_;
        if (readers_ == 0) {
            write_cv_.notify_one();
        }
    }
};
```

### Memory Ordering and Mutexes

Mutex operations provide implicit memory ordering guarantees, which is crucial for correct concurrent programming. You don't need explicit atomics when using mutexes.

**Acquire-Release semantics:**
- `lock()` provides **acquire semantics**: Ensures no memory operations before the lock are reordered after it
- `unlock()` provides **release semantics**: Ensures all memory operations before the unlock complete before it
- This creates a synchronization point between threads

**Why this matters:**
- Prevents subtle bugs from instruction reordering
- Ensures visibility of shared data changes
- Makes mutex-based synchronization safe without additional memory barriers
- The compiler and CPU cannot reorder operations across lock/unlock boundaries

**Practical implication:** When you use mutexes correctly, you get thread-safe memory access "for free" - no need for atomic operations on the protected data.

Mutex operations provide acquire-release semantics:

```cpp
// lock() provides acquire semantics
void lock() {
    native_lock();
    // Memory barrier: ensures no reordering of loads/stores after lock
    std::atomic_thread_fence(std::memory_order_acquire);
}

// unlock() provides release semantics
void unlock() {
    // Memory barrier: ensures all writes complete before unlock
    std::atomic_thread_fence(std::memory_order_release);
    native_unlock();
}
```

### Spinlock Alternative

Spinlocks are a lightweight alternative to mutexes that use busy-waiting instead of blocking. They can be faster for very short critical sections but waste CPU cycles if the lock is held for long.

**Key characteristics:**
- Uses `std::atomic_flag` (the lock-free primitive)
- Continuously checks the flag in a loop (spinning)
- No OS context switches or kernel transitions
- Minimal overhead when uncontended

**When to use spinlocks:**
- Critical sections are extremely short (few instructions)
- Lock is held for very brief periods
- Running on real-time systems with strict timing requirements
- Low-contention scenarios where blocking overhead dominates

**When to avoid spinlocks:**
- Critical sections are long or unpredictable
- High contention (many threads competing)
- Power-constrained environments (spinning wastes energy)
- General-purpose applications (mutexes are usually better)

**Note:** C++ does not provide a standard `std::spinlock`, but you can implement one as shown here using `std::atomic_flag`.

For very short critical sections, spinlocks can be used:

```cpp
class spinlock {
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Spin (busy wait)
        }
    }
    
    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};
```

### Performance Considerations

Understanding the performance characteristics of mutexes is crucial for writing efficient concurrent code.

**Contention:**
- High contention occurs when many threads frequently compete for the same lock
- Contention causes threads to block, reducing parallelism
- Solutions: reduce lock scope, use more granular locks, or lock-free algorithms

**Critical section size:**
- Keep critical sections as small as possible
- Move non-critical work outside the locked region
- Avoid I/O operations or expensive computations while holding locks

**Lock granularity:**
- **Coarse-grained:** One lock protects many data structures (simpler, more contention)
- **Fine-grained:** Many locks protect small pieces (complex, less contention)
- Balance based on your specific use case and access patterns

**Lock overhead:**
- Mutex operations have overhead (~50-100 nanoseconds on modern hardware)
- This overhead is negligible compared to blocking on contention
- Consider lock-free alternatives for extremely high-frequency operations

**Cache effects:**
- False sharing occurs when unrelated data shares the same cache line
- Can cause unnecessary cache invalidation between cores
- Solution: pad data structures or align to cache line boundaries

- **Contention**: High contention reduces performance
- **Critical section size**: Keep critical sections as small as possible
- **Lock granularity**: Fine-grained locks vs coarse-grained locks
- **Lock overhead**: Mutex operations have overhead (~50-100 nanoseconds)
- **Cache effects**: False sharing can degrade performance

### Fairness

Mutex fairness determines which waiting thread acquires the lock when it becomes available. Different implementations make different trade-offs between fairness and performance.

**Fairness types:**
- **FIFO fairness:** Threads acquire locks in the order they requested (fair but slower)
- **Priority fairness:** Higher priority threads are preferred (can cause priority inversion)
- **Unfair:** No guarantee (common for performance, can lead to starvation)

**Priority inversion:**
- Occurs when a high-priority thread waits for a low-priority thread holding a lock
- The low-priority thread can't run because medium-priority threads are running
- Solution: priority inheritance (shown in the example) or priority ceiling protocols

**Trade-offs:**
- Fair mutexes prevent starvation but have higher overhead
- Unfair mutexes are faster but some threads may wait indefinitely
- Most standard library mutexes are unfair for performance reasons
- Consider fairness requirements for real-time or mission-critical systems

Mutex implementations vary in fairness:

- **FIFO fairness**: Threads acquire in order
- **Priority fairness**: Higher priority threads preferred
- **Unfair**: No guarantee (common for performance)

```cpp
// POSIX mutex fairness
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);  // Priority inheritance
```

## Best Practices

1. **Always use RAII wrappers** (`lock_guard`, `unique_lock`) instead of manual lock/unlock to prevent deadlocks from exceptions or early returns

2. **Keep critical sections as small as possible** - only protect the actual shared data access, not entire functions

3. **Avoid nested locks when possible** - they can lead to deadlocks and make code hard to reason about. If necessary, always acquire locks in a consistent order

4. **Use shared_mutex for read-heavy workloads** - it allows concurrent reads while maintaining exclusive write access

5. **Consider lock-free alternatives for high-contention scenarios** - atomic operations or lock-free data structures can outperform mutexes in some cases

6. **Profile to identify contention hotspots** - use profiling tools to find where locks are actually causing performance issues

7. **Use appropriate mutex type for the use case** - don't default to recursive_mutex; use the simplest mutex that meets your needs

8. **Be aware of priority inversion issues** with priority-based scheduling - use priority inheritance when available

9. **Document lock ordering** - if you must use multiple locks, clearly document and enforce the acquisition order

10. **Avoid holding locks while calling unknown code** - callbacks or virtual functions may try to acquire the same lock, causing deadlock
