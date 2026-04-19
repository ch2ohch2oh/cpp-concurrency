# std::atomic_ref (C++20)

## Motivation

`std::atomic_ref` allows you to apply atomic operations to existing non-atomic objects without requiring them to be declared as `std::atomic`. This is particularly useful when you need atomic access to objects that are part of existing data structures, shared memory, or legacy code that cannot be easily modified.

The key motivation is to enable atomic operations on arbitrary objects:
- **No object modification**: Apply atomic semantics without changing the original object type
- **Flexibility**: Works with existing objects, arrays, and data structures
- **Performance**: Avoids copying or wrapping objects in atomic wrappers
- **Interoperability**: Useful for shared memory and hardware interfaces
- **Convenience**: Simplifies adding atomic access to legacy code

`std::atomic_ref` is particularly useful for:
- Applying atomic operations to members of structs/classes
- Making array elements atomic without changing the array type
- Shared memory structures in multi-process applications
- Legacy codebases where object types cannot be changed
- Performance-critical code where object copying is expensive

## Practical Usage

See the `examples/atomic-ref/` directory for complete working examples:
- `01-basic-atomic-ref.cpp` - Basic atomic operations on non-atomic objects
- `02-compare-exchange.cpp` - Lock-free compare-and-swap operations

### Key Features

**Reference Semantics**: `std::atomic_ref` is a reference wrapper that provides atomic operations on the referenced object without modifying its type.

**No Allocation**: Unlike `std::atomic`, `std::atomic_ref` doesn't allocate memory - it's just a view on existing data.

**Type Requirements**: The referenced type must be trivially copyable and meet alignment requirements for atomic operations.

**Lifetime**: The referenced object must outlive the `atomic_ref` - this is the programmer's responsibility.

### Common Patterns

**Basic Atomic Operations**: Use `load`, `store`, `fetch_add`, `fetch_sub`, etc., on regular non-atomic objects.

**Struct Members**: Apply atomic operations to individual members of structs without making the entire struct atomic.

**Arrays**: Create atomic references to array elements for lock-free parallel processing.

**Compare-Exchange**: Implement lock-free algorithms using compare-exchange on regular objects.

## Pros

- **No object modification**: Can make existing objects atomic without changing their type
- **Zero overhead**: No allocation or copying - just a reference wrapper
- **Flexibility**: Works with any trivially copyable type
- **Performance**: Avoids the overhead of wrapping objects
- **Interoperability**: Useful for shared memory and hardware interfaces
- **Standard API**: Uses the same interface as `std::atomic`

## Cons

- **Lifetime management**: User must ensure the referenced object outlives the atomic_ref
- **Type restrictions**: Only works with trivially copyable types with proper alignment
- **No storage**: Doesn't own the object, just provides atomic access
- **Alignment requirements**: Object must be properly aligned for atomic operations
- **Limited use cases**: Most new code should use `std::atomic` directly
- **C++20 only**: Requires a C++20-compatible compiler

## Underlying Implementation

### atomic_ref Implementation

`std::atomic_ref` is implemented as a lightweight wrapper that casts the referenced object's address to an atomic pointer and delegates operations to the atomic functions:

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

For types that don't support hardware atomic operations, a lock-based fallback can be used:

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

1. **Ensure object lifetime**: The referenced object must outlive the atomic_ref
2. **Verify alignment**: Check that the referenced object has proper alignment for atomic operations
3. **Prefer atomic<T>**: Use atomic_ref only when you cannot change the object type
4. **Check lock-freedom**: Use `is_lock_free()` for performance-critical code
5. **Use appropriate memory ordering**: Choose the weakest ordering that provides correctness
6. **Avoid storing atomic_ref**: Create on stack when needed to avoid lifetime issues
7. **Be careful in classes**: Don't store atomic_ref as a member due to potential lifetime issues
8. **Test on target platforms**: Implementation varies across architectures
9. **Document requirements**: Clearly document which objects require atomic access
10. **Profile performance**: atomic_ref may have different performance than atomic<T>
