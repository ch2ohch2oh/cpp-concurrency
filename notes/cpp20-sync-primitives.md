# C++20 Synchronization Primitives: Usage and Implementation

## History

C++20 introduced three new synchronization primitives that had been missing from the standard library for years:

**std::latch**
- A one-time synchronization barrier that allows threads to wait until a counter reaches zero
- Previously required manual implementation using condition variables or external libraries
- Standardized in C++20 (2020) as part of the parallelism and concurrency enhancements

**std::barrier**
- A reusable synchronization barrier for phased algorithms
- Similar to latches but can be used multiple times with optional completion functions
- Also added in C++20, addressing a long-standing gap in the standard library

**std::semaphore**
- Counting and binary semaphores for resource management
- Semaphores have been available in POSIX and Windows APIs for decades
- Finally standardized in C++20, providing a portable semaphore implementation

These primitives fill critical gaps in the C++ synchronization toolbox, which previously relied heavily on mutexes and condition variables for all synchronization patterns.

## Motivation

Before C++20, programmers had to implement these patterns manually using `std::mutex`, `std::condition_variable`, and atomic operations. This led to:

- **Error-prone code**: Manual implementations often had subtle bugs (spurious wakeups, missed notifications)
- **Inconsistent APIs**: Different libraries used different conventions
- **Performance overhead**: Generic condition-variable-based implementations were less efficient than platform-specific primitives
- **Portability issues**: Code using POSIX semaphores or Windows events wasn't portable

The C++20 primitives provide:
- **Correctness**: Tested, standard implementations
- **Performance**: Can use optimized platform-specific mechanisms (futex on Linux, kernel objects on Windows)
- **Expressiveness**: Clear intent through well-named types
- **Portability**: Standard API across all platforms

## Common Use Cases

### std::latch - One-Time Barrier

**Use Cases:**
- Initialization phases where multiple threads must complete before proceeding
- Fork-join parallelism: wait for all worker threads to finish
- One-shot coordination points in parallel algorithms
- Startup synchronization in multi-threaded applications

**Pros:**
- Simple and intuitive API
- Cannot be accidentally reused (prevents logic errors)
- Efficient implementation using atomics and condition variables
- No risk of deadlock if used correctly

**Cons:**
- Single-use only - must create a new latch for each synchronization point
- Cannot signal completion in batches (only decrement)
- No timeout support on wait()

**Basic usage:**
```cpp
std::latch done(4);  // Wait for 4 threads

// In each worker thread:
done.count_down();  // Signal completion

// In main thread:
done.wait();  // Block until count reaches 0
```

**Full example:** See `examples/cpp20-sync-primitives/01-latch-basic.cpp`

### std::barrier - Reusable Barrier

**Use Cases:**
- Phased parallel algorithms (e.g., iterative computations, stencil codes)
- Time-stepping simulations where all threads must synchronize at each step
- Parallel algorithms with multiple synchronization points
- Bulk synchronous parallel (BSP) model implementations

**Pros:**
- Reusable - can synchronize multiple phases with a single barrier
- Completion function runs after each phase (useful for cleanup or coordination)
- Efficient for repeated synchronization
- Supports different numbers of arriving threads

**Cons:**
- More complex than latch
- Requires careful design of completion functions (must be noexcept)
- Can be misused if phases don't complete correctly
- No timeout support

**Basic usage:**
```cpp
std::barrier sync(4);  // Use default completion function

// In each thread:
sync->arrive_and_wait();  // Synchronize at each phase
```

**Note:** Pass barrier by pointer to threads since it's not copyable.

**Full example:** See `examples/cpp20-sync-primitives/02-barrier-basic.cpp`

### std::counting_semaphore - Resource Pooling

**Use Cases:**
- Limiting concurrent access to a resource (e.g., database connections, file handles)
- Producer-consumer patterns with bounded buffers
- Rate limiting (e.g., API call throttling)
- Thread pool work queue management

**Pros:**
- Flexible counting mechanism (can limit to N concurrent operations)
- Timeout support with `try_acquire_for()`
- Can be used as a binary semaphore (mutex-like)
- No ownership semantics (unlike mutex, can release from different thread)

**Cons:**
- No ownership tracking (can lead to deadlocks if misused)
- Requires careful management of acquire/release pairs
- Can be less efficient than mutex for simple mutual exclusion
- Template parameter for max value can be confusing

**Basic usage:**
```cpp
// Template parameter: max value (upper bound)
// Constructor argument: initial value (available resources)
std::counting_semaphore<3> sem(3);  // Max 3, start with 3 available

sem.acquire();  // Wait for available slot
// ... use resource ...
sem.release();  // Release slot
```

**Full example:** See `examples/cpp20-sync-primitives/03-semaphore-counting.cpp`

### std::binary_semaphore - Mutex Alternative

**Use Cases:**
- Simple mutual exclusion when mutex ownership isn't needed
- Signaling between threads (event-like behavior)
- Lock-free data structures as a fallback
- Situations where lock release must happen from a different thread

**Pros:**
- Simpler than mutex (no ownership, no recursive locking)
- Can be released from any thread
- Useful for signaling patterns
- Template specialization of counting_semaphore

**Cons:**
- No ownership semantics (easy to misuse)
- No recursive locking
- No try_lock semantics
- Generally prefer mutex for mutual exclusion

**Basic usage:**
```cpp
std::binary_semaphore sem(1);  // Binary semaphore (1 or 0)

sem.acquire();  // Enter critical section
// ... critical code ...
sem.release();  // Exit critical section
```

**Full example:** See `examples/cpp20-sync-primitives/04-semaphore-binary.cpp`

### Semaphore with Timeout

**Use Cases:**
- Avoiding deadlocks by timing out on blocked operations
- Graceful degradation when resources are unavailable
- Polling for resource availability with backoff
- Implementing timeouts in producer-consumer scenarios

**Pros:**
- Prevents indefinite blocking
- Enables error handling for resource contention
- Useful for implementing responsive applications

**Cons:**
- Requires careful timeout value selection
- Can lead to resource leaks if not handled correctly
- May mask deeper synchronization problems

**Basic usage:**
```cpp
std::counting_semaphore<1> sem(0);

if (sem.try_acquire_for(std::chrono::seconds(1))) {
    // Acquired successfully
} else {
    // Timeout - handle gracefully
}
```

**Full example:** See `examples/cpp20-sync-primitives/05-semaphore-timeout.cpp`

### Thread Pool with Semaphore

**Use Cases:**
- Bounded work queues to prevent memory exhaustion
- Backpressure in producer-consumer systems
- Managing concurrent task submission
- Implementing graceful degradation under load

**Pros:**
- Limits queue size with counting semaphore
- Efficient signaling with semaphores
- Clean separation of concerns

**Cons:**
- More complex than unbounded queue
- Requires careful shutdown handling
- May reject tasks under heavy load

**Example:** See `examples/cpp20-sync-primitives/06-threadpool-semaphore.cpp`

## Underlying Implementation

### std::latch Implementation Concepts

The latch is implemented using an atomic counter combined with a condition variable:

- **Atomic counter**: Tracks the number of threads yet to arrive. Uses `fetch_sub` with acquire-release semantics to ensure visibility.
- **Condition variable**: Blocks waiting threads when the counter is non-zero. The last thread to decrement calls `notify_all()` to wake all waiters.
- **Memory ordering**: Acquire-release semantics ensure that all writes before `count_down()` are visible to threads after `wait()` returns.

**Key insight:** The latch can use a fast path: if the counter is already zero, `wait()` returns immediately without acquiring the mutex. This avoids unnecessary synchronization when all threads have already arrived.

**Full implementation:**
```cpp
#include <atomic>
#include <condition_variable>
#include <mutex>

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

### std::barrier Implementation Concepts

The barrier is more complex due to reusability and completion functions:

- **Phase counter**: Tracks the current phase number to distinguish between different synchronization points.
- **Atomic arrival counter**: Decrements as threads arrive. The last thread resets it and advances the phase.
- **Completion function**: Runs exactly once per phase, after all threads have arrived but before they're released.
- **Two-step synchronization**: Threads must check both the arrival counter and the phase number to avoid waking up for the wrong phase.

**Key insight:** The barrier uses a "phase token" pattern. Each thread remembers the phase it arrived in and only wakes when the phase changes. This prevents threads from waking up prematurely or for old phases.

**Full implementation:**
```cpp
#include <atomic>
#include <condition_variable>
#include <mutex>

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

### std::counting_semaphore Implementation Concepts

The counting semaphore uses a classic producer-consumer pattern:

- **Atomic counter**: Represents available resources. Incremented by producers, decremented by consumers.
- **Condition variable**: Blocks consumers when the counter is zero.
- **Spurious wakeup handling**: The wait loop must re-check the counter after waking, as condition variables can wake spuriously.
- **Lock-free fast path**: `try_acquire()` uses compare-exchange without the mutex for the common case where resources are available.

**Key insight:** The implementation separates the "check" (is counter > 0?) from the "wait" (block if not). This allows `try_acquire_for()` to implement timeout semantics efficiently.

**Full implementation:**
```cpp
#include <atomic>
#include <condition_variable>
#include <mutex>

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

### std::binary_semaphore

Binary semaphore is simply a template specialization of counting_semaphore with a max value of 1:

```cpp
using binary_semaphore = counting_semaphore<1>;
```

This provides a type alias for clarity when you specifically need binary semantics.

### Platform-Specific Optimizations

**Linux Futex Implementation:**
- Uses the Linux `futex` system call for efficient waiting and waking
- Avoids the overhead of condition variables and mutexes
- Direct kernel support for sleeping on an address
- Only available on Linux systems

**Windows Semaphore Implementation:**
- Uses Windows kernel semaphore objects
- Leverages `CreateSemaphore`, `ReleaseSemaphore`, `WaitForSingleObject`
- Built-in timeout support via the Windows API
- Only available on Windows systems

These platform-specific implementations provide better performance than the generic condition-variable-based approach by using kernel primitives directly.

### Memory Ordering Considerations

Proper memory ordering is critical for correctness:

- **Release semantics** on `count_down()` and `release()`: Ensures all writes before the operation are visible to threads that subsequently observe the counter change.
- **Acquire semantics** on `wait()` and `acquire()`: Ensures the thread sees all writes from the thread that last modified the counter.
- **Relaxed ordering** for internal operations: When the counter is only used for synchronization (not data), relaxed ordering suffices.

**Common pattern:** Use acquire-release for the counter updates, and relaxed for reads that don't need synchronization.

### Spurious Wakeup Handling

All condition-variable-based implementations must handle spurious wakeups:

```cpp
while (counter_.load(std::memory_order_acquire) == 0) {
    cv_.wait(lock);  // May wake spuriously
}
```

The loop re-checks the condition after each wakeup, ensuring the thread only proceeds when the condition is actually satisfied.

### Latch vs Barrier Comparison

**Latch:**
- One-time use only
- Cannot be reused after counter reaches zero
- Simpler implementation
- Suitable for one-shot coordination

**Barrier:**
- Reusable across multiple phases
- Supports completion functions
- More complex implementation
- Suitable for iterative algorithms

**Key difference:** Latch is "fire and forget" - once it's triggered, it's done. Barrier is "cyclic" - it can be used indefinitely for repeated synchronization.

## Best Practices

1. **Use latch for one-time synchronization** (e.g., initialization, fork-join patterns)
2. **Use barrier for phased algorithms** (e.g., iterative computations, time-stepping simulations)
3. **Use semaphore for resource pooling** (e.g., connection limits, bounded queues)
4. **Prefer counting_semaphore over manual condition variables** - it's simpler and less error-prone
5. **Be aware that latch cannot be reused** - create a new one for each synchronization point
6. **Barrier completion functions should be noexcept** - they run in a critical synchronization context
7. **Use try_acquire with timeout to avoid deadlocks** - provides a safety mechanism for resource contention
8. **Binary semaphore can replace mutex for simple cases** - but prefer mutex when ownership semantics matter
9. **Consider fairness when choosing between primitives** - some implementations may have fairness guarantees
10. **Test on target platforms** - implementation details may vary between compilers and operating systems
11. **Prefer higher-level abstractions** when available (e.g., parallel algorithms, thread pools)
12. **Profile before optimizing** - mutex-based solutions are often fast enough for most use cases
