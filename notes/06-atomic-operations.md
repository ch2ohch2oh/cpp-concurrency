# Atomic Operations: Usage and Implementation

## Practical Usage

### Basic Atomic Operations

Atomic operations provide a way to perform read-modify-write operations that are indivisible from the perspective of other threads. This means no thread can see a partially updated value.

The most common atomic operation is `fetch_add`, which atomically adds a value to the atomic variable and returns the previous value. This is useful for counters where multiple threads need to increment a shared value.

```cpp
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> counter(0);

void increment() {
    counter.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter.load() << "\n";
    return 0;
}
```

In this example, `memory_order_relaxed` is used because we only need atomicity—no synchronization with other memory operations is required.

### Memory Order Defaults

If you don't specify a memory order parameter, all atomic operations default to `std::memory_order_seq_cst` (sequentially consistent). This provides the strongest guarantees:
- All operations appear in a total order that all threads agree on
- Acts as both acquire and release semantics
- Includes full memory barriers

For example, these are equivalent:
```cpp
counter.fetch_add(1);  // Uses memory_order_seq_cst by default
counter.fetch_add(1, std::memory_order_seq_cst);  // Explicit
```

Sequential consistency is the safest choice but has the highest performance cost. Use weaker orders like `memory_order_relaxed`, `memory_order_acquire`, or `memory_order_release` when you don't need the full guarantees.

### Atomic Flags

`std::atomic_flag` is a special atomic type that is guaranteed to be lock-free. It provides only two operations: `test_and_set` and `clear`. This makes it ideal for simple spinlocks and as a building block for more complex synchronization primitives.

```cpp
#include <atomic>
#include <thread>

std::atomic_flag flag = ATOMIC_FLAG_INIT;

void worker() {
    while (!flag.test_and_set(std::memory_order_acquire)) {
        // Spin until we can set the flag
    }
    // Critical section
    flag.clear(std::memory_order_release);
}

int main() {
    std::thread t1(worker);
    std::thread t2(worker);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

The `test_and_set` operation atomically sets the flag to true and returns the previous value. If it was false, the thread now "owns" the lock. The `clear` operation releases the lock by setting the flag back to false.

### Compare-Exchange Loop

The compare-exchange operation is the foundation of most lock-free algorithms. It atomically compares the atomic variable with an expected value, and if they match, updates it to a new value. If they don't match, it updates the expected value with the actual current value.

This is typically used in a loop to retry the operation if another thread modified the value in the meantime:

```cpp
#include <atomic>
#include <iostream>

std::atomic<int> value(10);

void update_to_new_value(int old_val, int new_val) {
    int expected = old_val;
    while (!value.compare_exchange_weak(expected, new_val,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
        // expected was updated with the actual value
        // Retry if it still matches old_val
        if (expected != old_val) {
            std::cout << "Value changed from " << old_val 
                      << " to " << expected << "\n";
            break;
        }
    }
}

int main() {
    update_to_new_value(10, 20);
    std::cout << "Final value: " << value.load() << "\n";
    return 0;
}
```

Note that we use `compare_exchange_weak` which can fail spuriously (even when the values match). This is acceptable in loops and can be more efficient on some architectures.

### Atomic Pointers

Atomic operations work on pointers as well as integral types. This enables lock-free data structures where nodes are linked together using atomic pointers. The classic example is a lock-free stack implemented as a singly-linked list.

```cpp
#include <atomic>
#include <memory>

struct Node {
    int data;
    Node* next;
};

std::atomic<Node*> head(nullptr);

void push(int value) {
    Node* new_node = new Node{value, nullptr};
    
    Node* old_head = head.load(std::memory_order_relaxed);
    do {
        new_node->next = old_head;
    } while (!head.compare_exchange_weak(old_head, new_node,
                                        std::memory_order_release,
                                        std::memory_order_relaxed));
}

int main() {
    push(1);
    push(2);
    push(3);
    
    return 0;
}
```

The key insight is that we read the current head, set our new node's next pointer to it, and then try to atomically swap the head to point to our new node. If another thread pushed a node in the meantime, the compare-exchange fails and we retry with the updated head value.

### Atomic Shared Pointers

C++ provides free functions for atomic operations on `std::shared_ptr`. Since `shared_ptr` itself is not atomic (it contains reference counts that need to be updated atomically), these functions handle the atomic reference counting internally.

```cpp
#include <atomic>
#include <memory>

std::shared_ptr<int> global_data;

void update_data(int value) {
    auto new_data = std::make_shared<int>(value);
    std::atomic_store(&global_data, new_data);
}

void read_data() {
    auto data = std::atomic_load(&global_data);
    if (data) {
        std::cout << "Data: " << *data << "\n";
    }
}
```

This is particularly useful for implementing lock-free read-copy-update (RCU) patterns where readers can access data without blocking writers.

### Realistic Example: Lock-Free Counter

A lock-free counter is a practical use case for atomic operations. By using `memory_order_relaxed`, we avoid the overhead of memory barriers since we only care about the atomicity of the increment operation, not synchronization with other variables.

```cpp
#include <atomic>
#include <vector>
#include <thread>

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
```

This approach scales well because threads don't block each other—each increment is a single atomic instruction. However, the final count might not be visible to all threads immediately due to the relaxed memory order.

## Underlying Implementation

### Hardware Atomic Instructions

Atomic operations are not implemented in software alone—they rely on special CPU instructions that guarantee atomicity at the hardware level. These instructions ensure that the operation completes without interruption from other CPU cores.

- **x86**: Uses the LOCK prefix to lock the memory bus for the duration of the operation. Instructions like CMPXCHG (compare-exchange) and XADD (exchange-and-add) are used.
- **ARM**: Uses load-exclusive/store-exclusive pairs (LDREX/STREX on ARMv7, LDAXR/STLXR on ARMv8) to detect if another core modified the memory between the load and store.
- **Other architectures**: Each has its own mechanism, such as PowerPC's lwarx/stwcx instructions.

The C++ standard library abstracts these differences, providing a consistent interface across platforms.

### std::atomic Implementation

The standard library implementation of `std::atomic` typically delegates to compiler intrinsics, which in turn generate the appropriate hardware instructions. Here's a simplified conceptual implementation:

```cpp
// Simplified conceptual implementation
namespace std {
    template<typename T>
    class atomic {
    public:
        atomic() noexcept = default;
        constexpr atomic(T desired) noexcept : value_(desired) {}
        
        T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
            return __atomic_load_n(&value_, to_gcc_order(order));
        }
        
        void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
            __atomic_store_n(&value_, desired, to_gcc_order(order));
        }
        
        T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_exchange_n(&value_, desired, to_gcc_order(order));
        }
        
        bool compare_exchange_weak(T& expected, T desired,
                                   std::memory_order success,
                                   std::memory_order failure) noexcept {
            return __atomic_compare_exchange_n(&value_, &expected, desired,
                                             false, to_gcc_order(success),
                                             to_gcc_order(failure));
        }
        
        bool compare_exchange_strong(T& expected, T desired,
                                     std::memory_order success,
                                     std::memory_order failure) noexcept {
            return __atomic_compare_exchange_n(&value_, &expected, desired,
                                             true, to_gcc_order(success),
                                             to_gcc_order(failure));
        }
        
        T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_fetch_add(&value_, arg, to_gcc_order(order));
        }
        
        T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_fetch_sub(&value_, arg, to_gcc_order(order));
        }
        
        T fetch_and(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_fetch_and(&value_, arg, to_gcc_order(order));
        }
        
        T fetch_or(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_fetch_or(&value_, arg, to_gcc_order(order));
        }
        
        T fetch_xor(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
            return __atomic_fetch_xor(&value_, arg, to_gcc_order(order));
        }
        
    private:
        T value_;
        
        static int to_gcc_order(std::memory_order order) {
            switch (order) {
                case std::memory_order_relaxed: return __ATOMIC_RELAXED;
                case std::memory_order_consume: return __ATOMIC_CONSUME;
                case std::memory_order_acquire: return __ATOMIC_ACQUIRE;
                case std::memory_order_release: return __ATOMIC_RELEASE;
                case std::memory_order_acq_rel: return __ATOMIC_ACQ_REL;
                case std::memory_order_seq_cst: return __ATOMIC_SEQ_CST;
                default: return __ATOMIC_SEQ_CST;
            }
        }
    };
}
```

The actual implementation is more complex, handling alignment requirements, lock-based fallbacks for large types, and various platform-specific optimizations.

### x86 Assembly Implementation

x86 has a strong memory model, which means many operations don't require explicit memory barriers. The LOCK prefix ensures atomicity by locking the memory bus.

```asm
; load with acquire semantics
mov eax, [value]           ; Load value
; No memory barrier needed on x86 (already acquire)

; store with release semantics
mov [value], ebx           ; Store value
mfence                     ; Memory fence for release

; compare_exchange
mov eax, [expected]         ; Load expected value
lock cmpxchg [value], edx   ; Atomic compare and exchange
; ZF flag set if successful

; fetch_add
lock xadd [value], eax      ; Atomic add and return old value
```

Notice that for simple loads, x86 doesn't need any special instructions—the hardware guarantees acquire semantics. For stores that need release semantics, an `mfence` (memory fence) instruction may be needed.

### ARM Implementation

ARM has a weaker memory model than x86, so it requires explicit acquire/release semantics using special load-exclusive and store-exclusive instructions.

```asm
; load with acquire semantics
ldaxr w0, [value]          ; Load exclusive with acquire

; store with release semantics
stlxr w1, w0, [value]      ; Store exclusive with release
cbnz w1, retry             ; Retry if failed

; compare_exchange
ldaxr w0, [value]          ; Load exclusive
cmp w0, w2                 ; Compare with expected
b.ne failed
stlxr w3, w1, [value]      ; Store if equal
cbnz w3, retry             ; Retry if failed
```

The load-exclusive/store-exclusive pair is ARM's mechanism for implementing atomic operations. The store-exclusive checks if the memory location was modified since the load-exclusive, and fails if it was. This is why loops are needed—they retry the operation if another core interfered.

### std::atomic_flag Implementation

`std::atomic_flag` is the simplest atomic type, guaranteed to be lock-free on all platforms. Its implementation is straightforward:

```cpp
class atomic_flag {
public:
    atomic_flag() noexcept : flag_(false) {}
    
    bool test_and_set(std::memory_order order = std::memory_order_seq_cst) noexcept {
        return __atomic_test_and_set(&flag_, to_gcc_order(order));
    }
    
    void clear(std::memory_order order = std::memory_order_seq_cst) noexcept {
        __atomic_clear(&flag_, to_gcc_order(order));
    }
    
private:
    bool flag_;
};
```

Because it only supports test-and-set and clear operations, it can be implemented using a single atomic instruction on all platforms, making it the most portable and efficient synchronization primitive.

### Lock-Free vs Lock-Based Implementation

Not all atomic types can be implemented using hardware instructions. For types that are too large or don't have appropriate hardware support, the standard library may fall back to using internal locks.

```cpp
// Lock-based fallback for large types
template<typename T>
class atomic_large {
private:
    mutable std::mutex mtx_;
    T value_;
    
public:
    T load(std::memory_order) const {
        std::lock_guard<std::mutex> lock(mtx_);
        return value_;
    }
    
    void store(T desired, std::memory_order) {
        std::lock_guard<std::mutex> lock(mtx_);
        value_ = desired;
    }
    
    // ... other operations with mutex protection
};
```

You can check if an atomic type is lock-free using the `is_always_lock_free` static constant or the `is_lock_free()` member function. Typically, atomic operations on integers and pointers up to the word size are lock-free, while larger structures may use locks.

### Compare-Exchange Weak vs Strong

The difference between weak and strong compare-exchange is important for performance:

```cpp
// Weak version may fail spuriously
bool compare_exchange_weak(T& expected, T desired,
                           std::memory_order success,
                           std::memory_order failure) {
    // Can fail even if expected == value_
    // Useful for loops where spurious failure is acceptable
    return hardware_cas(&value_, &expected, desired);
}

// Strong version guarantees no spurious failures
bool compare_exchange_strong(T& expected, T desired,
                            std::memory_order success,
                            std::memory_order failure) {
    // Only fails if expected != value_
    // May use retry loop internally
    while (true) {
        if (compare_exchange_weak(expected, desired, success, failure)) {
            return true;
        }
        // Retry on spurious failure
    }
}
```

On some architectures (like ARM with load-exclusive/store-exclusive), spurious failures can occur due to cache line evictions or other events. The weak version accepts these failures, while the strong version retries internally. Use weak in loops and strong for single attempts.

### Alignment Requirements

For hardware atomic operations to work correctly, atomic variables must be properly aligned. Misaligned atomic variables can cause crashes or undefined behavior on some platforms.

```cpp
// Compiler ensures proper alignment
template<typename T>
struct alignas(sizeof(T) > 4 ? 8 : 4) atomic {
    T value_;
};

// Or use max_align_t
template<typename T>
struct alignas(std::max_align_t) atomic {
    T value_;
};
```

The compiler automatically handles alignment for `std::atomic` types. However, if you're using `std::atomic_ref` (C++20) to make atomic operations on existing objects, you must ensure those objects are properly aligned yourself.

### Cache Line Padding

False sharing occurs when multiple atomic variables that are frequently modified by different threads happen to reside on the same cache line. This causes unnecessary cache coherency traffic as CPUs invalidate each other's cache lines.

To prevent this, pad atomic variables to the cache line size (typically 64 bytes):

```cpp
struct alignas(64) PaddedCounter {
    std::atomic<uint64_t> count_;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
};
```

This ensures each counter has its own cache line, eliminating false sharing. Modern compilers and libraries provide similar optimizations internally for common atomic types.

### Wait-Free vs Lock-Free Progress Guarantees

Atomic operations provide different progress guarantees:

```cpp
// Lock-free: At least one thread makes progress
class lock_free_queue {
    // Implementation ensures system-wide progress
};

// Wait-free: All threads make progress
class wait_free_counter {
    std::atomic<uint64_t> count_;
public:
    void increment() {
        // Always completes in bounded time
        count_.fetch_add(1, std::memory_order_relaxed);
    }
};
```

- **Lock-free**: If multiple threads are attempting the operation, at least one will complete in a bounded number of steps. Individual threads might starve, but the system as a whole makes progress.
- **Wait-free**: Every thread completes in a bounded number of steps, regardless of what other threads are doing. This is stronger but harder to achieve.

Simple operations like `fetch_add` are typically wait-free, while complex data structures like lock-free queues are usually only lock-free.

### Atomic Operations on Shared Pointers

Atomic operations on `shared_ptr` require careful handling of reference counts. The implementation must ensure that reference counts are updated atomically to prevent use-after-free errors.

```cpp
// Implementation uses reference counting with atomic operations
template<typename T>
struct atomic_shared_ptr_control_block {
    std::atomic<long> ref_count_;
    std::atomic<long> weak_count_;
    T* ptr_;
};

template<typename T>
std::shared_ptr<T> atomic_load(const std::shared_ptr<T>* p) {
    // Increment reference count atomically
    auto* control = get_control_block(p);
    control->ref_count_.fetch_add(1, std::memory_order_acq_rel);
    return *p;
}

template<typename T>
void atomic_store(std::shared_ptr<T>* p, std::shared_ptr<T> r) {
    // Decrement old reference count atomically
    // Store new pointer
    auto old = *p;
    *p = r;
    if (old) {
        release_shared(old);
    }
}
```

The key challenge is ensuring the reference count is incremented before the pointer is read, and decremented after the new pointer is stored. This requires careful ordering of memory operations.

### Hardware-Specific Optimizations

Different CPU architectures have different memory models, requiring different implementations of atomic operations:

```cpp
// x86: No memory barriers needed for most operations
// (x86 has strong memory model)

// ARM: Requires explicit memory barriers
// ldaxr/stlxr for acquire/release

// PowerPC: Requires sync instructions
// lwsync for lightweight sync, sync for full sync

template<typename T>
class atomic_platform_specific {
public:
    void store(T desired, std::memory_order order) {
#if defined(__x86_64__)
        // No barrier needed for relaxed
        value_ = desired;
        if (order >= std::memory_order_release) {
            __builtin_ia32_mfence();
        }
#elif defined(__aarch64__)
        // Always use stlxr for release semantics
        __atomic_store_n(&value_, desired, __ATOMIC_RELEASE);
#endif
    }
};
```

The standard library implementation handles these differences internally, so your code works correctly regardless of the target platform. However, understanding these differences can help with performance tuning and debugging.

## Best Practices

1. **Use the weakest memory order** that provides the necessary guarantees. `memory_order_relaxed` is fastest but provides no synchronization. Use `memory_order_seq_cst` only when you need total ordering.

2. **Prefer atomic operations over mutexes** for simple counters and flags. They're typically faster and don't risk blocking.

3. **Be aware of false sharing** and use padding when necessary. Place frequently-modified atomic variables on separate cache lines.

4. **Use compare_exchange_weak in loops**, compare_exchange_strong for single attempts. Weak is more efficient on some architectures.

5. **Ensure atomic types are properly aligned**. The compiler handles this for `std::atomic`, but be careful with `atomic_ref`.

6. **Understand the difference between lock-free and wait-free**. Lock-free guarantees system-wide progress; wait-free guarantees per-thread progress.

7. **Use atomic_ref (C++20)** for atomic operations on existing objects without changing their type.

8. **Be cautious with atomic operations on large types**. They may use locks internally, defeating the purpose of using atomics.

9. **Profile to determine if atomic operations are faster than mutexes** for your specific use case. The answer isn't always obvious.

10. **Use atomic operations only when necessary**. They have overhead and can make code harder to reason about.
