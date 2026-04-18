# std::atomic_ref: Usage and Implementation

## Practical Usage

### Basic atomic_ref Usage

```cpp
#include <atomic>
#include <thread>
#include <iostream>

int main() {
    int value = 0;
    std::atomic_ref<int> atomic_value(value);
    
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i) {
            atomic_value.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i) {
            atomic_value.fetch_add(1, std::memory_order_relaxed);
        }
    });
    
    t1.join();
    t2.join();
    
    std::cout << "Final value: " << value << "\n";  // 2000
    return 0;
}
```

### atomic_ref with Existing Objects

```cpp
#include <atomic>
#include <vector>
#include <thread>

struct Data {
    int counter;
    double value;
};

void increment_counter(Data& data) {
    std::atomic_ref<int> atomic_counter(data.counter);
    atomic_counter.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::vector<Data> data(10);
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 10; ++i) {
        threads.emplace_back(increment_counter, std::ref(data[i]));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### atomic_ref with Arrays

```cpp
#include <atomic>
#include <array>
#include <thread>

int main() {
    std::array<int, 4> values = {0, 0, 0, 0};
    
    auto worker = [&](size_t index) {
        std::atomic_ref<int> atomic_value(values[index]);
        for (int i = 0; i < 1000; ++i) {
            atomic_value.fetch_add(1, std::memory_order_relaxed);
        }
    };
    
    std::thread t1(worker, 0);
    std::thread t2(worker, 1);
    std::thread t3(worker, 2);
    std::thread t4(worker, 3);
    
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    
    return 0;
}
```

### atomic_ref with Compare-Exchange

```cpp
#include <atomic>
#include <thread>

int shared_value = 0;

void try_update_to_42() {
    std::atomic_ref<int> atomic_value(shared_value);
    int expected = 0;
    
    while (!atomic_value.compare_exchange_weak(
        expected, 42,
        std::memory_order_acq_rel,
        std::memory_order_acquire)) {
        expected = 0;  // Reset expected if it changed
    }
}

int main() {
    std::thread t1(try_update_to_42);
    std::thread t2(try_update_to_42);
    
    t1.join();
    t2.join();
    
    std::cout << "Final value: " << shared_value << "\n";  // 42
    return 0;
}
```

### Realistic Example: Shared Memory Structure

```cpp
#include <atomic>
#include <thread>
#include <iostream>

struct SharedBuffer {
    int data[100];
    int write_index;
    int read_index;
};

void producer(SharedBuffer& buffer) {
    std::atomic_ref<int> write_idx(buffer.write_index);
    
    for (int i = 0; i < 100; ++i) {
        int pos = write_idx.fetch_add(1, std::memory_order_acq_rel) % 100;
        buffer.data[pos] = i;
    }
}

void consumer(SharedBuffer& buffer) {
    std::atomic_ref<int> read_idx(buffer.read_index);
    
    for (int i = 0; i < 100; ++i) {
        int pos = read_idx.fetch_add(1, std::memory_order_acq_rel) % 100;
        std::cout << "Read: " << buffer.data[pos] << "\n";
    }
}

int main() {
    SharedBuffer buffer = {{0}, 0, 0};
    
    std::thread p(producer, std::ref(buffer));
    std::thread c(consumer, std::ref(buffer));
    
    p.join();
    c.join();
    
    return 0;
}
```

## Underlying Implementation

### atomic_ref Implementation

```cpp
#include <atomic>
#include <type_traits>

namespace std {
    template<typename T>
    class atomic_ref {
    private:
        T* ptr_;
        
        // Check if T is suitable for atomic operations
        static constexpr bool is_lock_free() {
            return atomic<T>::is_always_lock_free;
        }
        
    public:
        atomic_ref(T& obj) : ptr_(&obj) {
            static_assert(is_always_lock_free || sizeof(T) <= sizeof(void*),
                         "T must be lock-free or fit in a word");
        }
        
        atomic_ref(const atomic_ref&) noexcept = default;
        atomic_ref& operator=(const atomic_ref&) = delete;
        
        T load(std::memory_order order = memory_order_seq_cst) const noexcept {
            return atomic_load_explicit(reinterpret_cast<atomic<T>*>(ptr_), order);
        }
        
        void store(T desired, std::memory_order order = memory_order_seq_cst) noexcept {
            atomic_store_explicit(reinterpret_cast<atomic<T>*>(ptr_), desired, order);
        }
        
        T exchange(T desired, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_exchange_explicit(reinterpret_cast<atomic<T>*>(ptr_), desired, order);
        }
        
        bool compare_exchange_weak(T& expected, T desired,
                                  std::memory_order success,
                                  std::memory_order failure) noexcept {
            return atomic_compare_exchange_weak_explicit(
                reinterpret_cast<atomic<T>*>(ptr_), &expected, desired, success, failure);
        }
        
        bool compare_exchange_strong(T& expected, T desired,
                                     std::memory_order success,
                                     std::memory_order failure) noexcept {
            return atomic_compare_exchange_strong_explicit(
                reinterpret_cast<atomic<T>*>(ptr_), &expected, desired, success, failure);
        }
        
        T fetch_add(T arg, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_fetch_add_explicit(reinterpret_cast<atomic<T>*>(ptr_), arg, order);
        }
        
        T fetch_sub(T arg, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_fetch_sub_explicit(reinterpret_cast<atomic<T>*>(ptr_), arg, order);
        }
        
        T fetch_and(T arg, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_fetch_and_explicit(reinterpret_cast<atomic<T>*>(ptr_), arg, order);
        }
        
        T fetch_or(T arg, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_fetch_or_explicit(reinterpret_cast<atomic<T>*>(ptr_), arg, order);
        }
        
        T fetch_xor(T arg, std::memory_order order = memory_order_seq_cst) noexcept {
            return atomic_fetch_xor_explicit(reinterpret_cast<atomic<T>*>(ptr_), arg, order);
        }
        
        T operator++(int) noexcept {
            return fetch_add(1);
        }
        
        T operator--(int) noexcept {
            return fetch_sub(1);
        }
        
        T operator++() noexcept {
            return fetch_add(1) + 1;
        }
        
        T operator--() noexcept {
            return fetch_sub(1) - 1;
        }
        
        T operator+=(T arg) noexcept {
            return fetch_add(arg) + arg;
        }
        
        T operator-=(T arg) noexcept {
            return fetch_sub(arg) - arg;
        }
        
        T operator&=(T arg) noexcept {
            return fetch_and(arg) & arg;
        }
        
        T operator|=(T arg) noexcept {
            return fetch_or(arg) | arg;
        }
        
        T operator^=(T arg) noexcept {
            return fetch_xor(arg) ^ arg;
        }
        
        static constexpr bool is_always_lock_free = atomic<T>::is_always_lock_free;
        bool is_lock_free() const noexcept {
            return atomic_is_lock_free_explicit(reinterpret_cast<atomic<T>*>(ptr_));
        }
    };
}
```

### Lock-Based Fallback

```cpp
// For types that don't support hardware atomic operations
template<typename T>
class atomic_ref_lock_based {
private:
    T* ptr_;
    mutable std::mutex mtx_;
    
public:
    atomic_ref_lock_based(T& obj) : ptr_(&obj) {}
    
    T load(std::memory_order) const noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        return *ptr_;
    }
    
    void store(T desired, std::memory_order) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        *ptr_ = desired;
    }
    
    bool compare_exchange_weak(T& expected, T desired,
                              std::memory_order, std::memory_order) noexcept {
        std::lock_guard<std::mutex> lock(mtx_);
        if (*ptr_ == expected) {
            *ptr_ = desired;
            return true;
        }
        expected = *ptr_;
        return false;
    }
};
```

### Alignment Requirements

```cpp
// atomic_ref requires proper alignment
template<typename T>
class atomic_ref {
public:
    static constexpr size_t required_alignment = 
        std::max(alignof(T), alignof(std::atomic<T>));
    
    atomic_ref(T& obj) {
        static_assert(alignof(T) >= alignof(std::atomic<T>) ||
                     reinterpret_cast<uintptr_t>(&obj) % alignof(std::atomic<T>) == 0,
                     "Referenced object must be properly aligned");
    }
};
```

### Memory Ordering Implementation

```cpp
// atomic_ref respects memory ordering
template<typename T>
T atomic_ref<T>::load(std::memory_order order) const noexcept {
    switch (order) {
        case memory_order_relaxed:
            return __atomic_load_n(ptr_, __ATOMIC_RELAXED);
        case memory_order_consume:
            return __atomic_load_n(ptr_, __ATOMIC_CONSUME);
        case memory_order_acquire:
            return __atomic_load_n(ptr_, __ATOMIC_ACQUIRE);
        case memory_order_seq_cst:
            return __atomic_load_n(ptr_, __ATOMIC_SEQ_CST);
        default:
            return __atomic_load_n(ptr_, __ATOMIC_SEQ_CST);
    }
}
```

### Platform-Specific Implementation

```cpp
// x86 implementation
template<typename T>
T atomic_ref<T>::load(std::memory_order order) const noexcept {
    if constexpr (sizeof(T) <= 4) {
        return *reinterpret_cast<volatile T*>(ptr_);  // x86 loads are acquire
    } else {
        // Use larger atomic operations
        return __atomic_load_n(ptr_, to_gcc_order(order));
    }
}

// ARM implementation
template<typename T>
T atomic_ref<T>::load(std::memory_order order) const noexcept {
    if (order == memory_order_relaxed) {
        return __atomic_load_n(ptr_, __ATOMIC_RELAXED);
    } else {
        return __atomic_load_n(ptr_, __ATOMIC_ACQUIRE);
    }
}
```

### Lifetime Considerations

```cpp
// atomic_ref must not outlive the referenced object
class Example {
private:
    int value_;
    std::atomic_ref<int> atomic_value_;  // DANGER!
    
public:
    Example() : value_(0), atomic_value_(value_) {}
    // atomic_value_ becomes dangling if Example is moved or copied
};

// Correct usage
class CorrectExample {
private:
    int value_;
    
public:
    CorrectExample() : value_(0) {}
    
    void increment() {
        std::atomic_ref<int> atomic_value(value_);
        atomic_value.fetch_add(1, std::memory_order_relaxed);
    }
};
```

### Type Requirements

```cpp
// atomic_ref requires trivially copyable types
template<typename T>
class atomic_ref {
    static_assert(std::is_trivially_copyable_v<T>,
                 "T must be trivially copyable");
    static_assert(std::is_object_v<T>,
                 "T must be an object type");
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>,
                 "T must not be const or volatile");
};
```

### Comparison with std::atomic

```cpp
// std::atomic - owns the storage
std::atomic<int> atomic_value(42);
atomic_value.store(10);

// std::atomic_ref - references existing storage
int value = 42;
std::atomic_ref<int> atomic_value_ref(value);
atomic_value_ref.store(10);
// value is now 10

// Use atomic_ref when:
// - Storage is already allocated
// - Need atomic access to existing objects
// - Can't change type to atomic<T>
```

## Best Practices

1. Ensure referenced object outlives atomic_ref
2. Verify proper alignment of referenced object
3. Use atomic_ref only when necessary (prefer atomic<T>)
4. Be aware of lock-based fallback for large types
5. Check is_lock_free() for performance-critical code
6. Use appropriate memory ordering
7. Don't store atomic_ref (create on stack when needed)
8. Be careful with object lifetime in classes
9. Test on target platforms (implementation varies)
10. Document atomic access requirements clearly
