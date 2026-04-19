# std::atomic::wait and notify (C++20)

## Motivation

C++20 introduced `wait` and `notify` operations on `std::atomic` to provide efficient synchronization without the overhead of condition variables. These operations allow threads to wait for atomic variables to change value, using efficient kernel-level mechanisms like futex on Linux.

The key motivation is to provide a simpler and more efficient alternative to condition variables:
- **Efficiency**: Uses kernel-level wait mechanisms (futex) instead of userspace condition variables
- **Simplicity**: No need for mutexes - works directly on atomic variables
- **Performance**: Avoids mutex overhead and spurious wakeups
- **Scalability**: Better for high-contention scenarios
- **Standardization**: Provides a portable API across platforms

`atomic::wait/notify` is particularly useful for:
- Producer-consumer patterns with atomic flags
- Lock-free data structures
- Simple signal/notification patterns
- High-performance synchronization primitives
- Scenarios where condition variables are overkill

## Practical Usage

See the `examples/atomic-wait-notify/` directory for complete working examples:
- `01-basic-wait.cpp` - Basic wait/notify pattern
- `02-producer-consumer.cpp` - Producer-consumer using atomic wait/notify

### Key Features

**wait**: Blocks the thread until the atomic value changes from the expected value. Uses efficient kernel mechanisms like futex on Linux.

**notify_one**: Wakes up one thread waiting on the atomic variable.

**notify_all**: Wakes up all threads waiting on the atomic variable.

**wait_for**: Wait with a timeout, returns status indicating whether notification was received or timeout occurred.

### Common Patterns

**Simple Signal**: Use atomic flag to signal between threads without mutexes.

**Producer-Consumer**: Use atomic flags to signal data availability in lock-free queues.

**Barrier Synchronization**: Wait for a counter to reach a specific value.

**Timeout Handling**: Use wait_for to avoid indefinite blocking.

## Pros

- **Efficient**: Uses kernel-level mechanisms (futex) for efficient waiting
- **No mutex overhead**: Works directly on atomic variables without mutexes
- **Simpler API**: Easier to use than condition variables for simple cases
- **Better performance**: Lower overhead than condition variables for simple synchronization
- **Portable**: Standard API across platforms with platform-specific optimizations
- **Scalable**: Better for high-contention scenarios

## Cons

- **Limited to atomics**: Only works with atomic types, not arbitrary conditions
- **Platform dependence**: Relies on OS support (futex on Linux, WaitOnAddress on Windows)
- **Less flexible**: No built-in predicate support (must use loops)
- **Spurious wakeups**: Still possible, need to handle with loops or predicates
- **C++20 only**: Requires a C++20-compatible compiler
- **Complex fallback**: Platforms without futex support need fallback implementations

## Underlying Implementation

### atomic::wait Implementation

The `wait` operation checks if the atomic value has changed, and if not, blocks the thread using kernel-level mechanisms:

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

On Linux, atomic wait/notify uses the futex system call for efficient kernel-level waiting:

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

1. **Use when mutex isn't needed**: atomic::wait is ideal for simple flag-based synchronization
2. **Handle spurious wakeups**: Always use the predicate version or wrap in a loop
3. **Choose notify wisely**: Use notify_one for single consumer, notify_all for multiple
4. **Consider platform support**: Be aware of platform-specific implementations (futex, WaitOnAddress)
5. **Use timeouts**: Use wait_for to avoid indefinite blocking in critical code
6. **Ensure proper memory ordering**: Use appropriate memory orders around wait/notify
7. **Compare with condition variables**: Use condition variables for complex predicates
8. **Test on target platforms**: Behavior may vary across platforms
9. **Document conditions**: Clearly document wait conditions and invariants
10. **Profile performance**: atomic::wait may have different performance characteristics than condition variables
