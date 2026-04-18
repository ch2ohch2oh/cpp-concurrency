# Lock Wrappers: Usage and Implementation

## Practical Usage

### std::lock_guard - Basic RAII Wrapper

```cpp
#include <mutex>
#include <iostream>

std::mutex mtx;
int shared_data = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);  // Lock on construction
    ++shared_data;
    // Automatic unlock on destruction
}

int main() {
    increment();
    std::cout << shared_data << "\n";
    return 0;
}
```

### std::unique_lock - Flexible RAII Wrapper

```cpp
#include <mutex>
#include <iostream>

std::mutex mtx;
int shared_data = 0;

void flexible_operation() {
    std::unique_lock<std::mutex> lock(mtx);  // Lock immediately
    
    // Can unlock early
    lock.unlock();
    
    // Do some non-critical work
    
    // Can lock again
    lock.lock();
    ++shared_data;
    
    // Can transfer ownership
    std::unique_lock<std::mutex> lock2 = std::move(lock);
}

int main() {
    flexible_operation();
    return 0;
}
```

### std::scoped_lock - Multiple Mutex Locking (C++17)

```cpp
#include <mutex>
#include <iostream>

std::mutex mtx1;
std::mutex mtx2;
int data1 = 0;
int data2 = 0;

void transfer(int value) {
    // Lock both mutexes atomically with deadlock avoidance.
    // std::scoped_lock uses std::lock() internally which tries different
    // locking orders and backs off on contention to prevent deadlocks.
    // Both mutexes are automatically unlocked when 'lock' goes out of scope.
    std::scoped_lock lock(mtx1, mtx2);
    
    data1 -= value;
    data2 += value;
}

int main() {
    transfer(10);
    std::cout << data1 << ", " << data2 << "\n";
    return 0;
}
```

### std::shared_lock - Shared Access (C++17)

```cpp
#include <shared_mutex>
#include <vector>

std::shared_mutex shared_mtx;
std::vector<int> data;

void read_data() {
    std::shared_lock<std::shared_mutex> lock(shared_mtx);
    // Multiple readers can hold this lock simultaneously
    for (int val : data) {
        // Read operation
    }
}

void write_data(int value) {
    std::unique_lock<std::shared_mutex> lock(shared_mtx);
    // Exclusive access for writing
    data.push_back(value);
}
```

### Deferred Locking

```cpp
#include <mutex>
#include <iostream>

std::mutex mtx;

void deferred_example() {
    // Construct without locking
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    
    // Do some preparation work
    
    // Lock when needed
    lock.lock();
    
    // Critical section
    
    // Automatic unlock
}
```

### Try Lock with Timeout

```cpp
#include <mutex>
#include <chrono>
#include <iostream>

std::timed_mutex mtx;

void try_lock_example() {
    std::unique_lock<std::timed_mutex> lock(mtx, std::defer_lock);
    
    if (lock.try_lock_for(std::chrono::milliseconds(100))) {
        // Lock acquired
        std::cout << "Lock acquired\n";
    } else {
        // Lock not acquired
        std::cout << "Failed to acquire lock\n";
    }
}
```

### Realistic Example: Thread-Safe Cache

```cpp
#include <mutex>
#include <unordered_map>
#include <string>

template<typename Key, typename Value>
class ThreadSafeCache {
private:
    std::unordered_map<Key, Value> cache_;
    // mutable allows locking in const member functions (get, size)
    mutable std::shared_mutex mtx_;
    
public:
    Value get(const Key& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        return Value();  // Default value
    }
    
    void set(const Key& key, const Value& value) {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        cache_[key] = value;
    }
    
    bool remove(const Key& key) {
        std::unique_lock<std::shared_mutex> lock(mtx_);
        return cache_.erase(key) > 0;
    }
    
    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        return cache_.size();
    }
};
```

## Underlying Implementation

### std::lock_guard Implementation

```cpp
// Simplified implementation
namespace std {
    template<class Mutex>
    class lock_guard {
    public:
        explicit lock_guard(Mutex& m) : mutex_(m) {
            mutex_.lock();
        }
        
        lock_guard(Mutex& m, std::adopt_lock_t) : mutex_(m) {
            // Already locked, don't lock again
        }
        
        ~lock_guard() {
            mutex_.unlock();
        }
        
        lock_guard(const lock_guard&) = delete;
        lock_guard& operator=(const lock_guard&) = delete;
        
    private:
        Mutex& mutex_;
    };
}
```

### std::unique_lock Implementation

```cpp
// Simplified implementation
namespace std {
    template<class Mutex>
    class unique_lock {
    public:
        explicit unique_lock(Mutex& m) 
            : mutex_(&m), owns_lock_(true) {
            mutex_->lock();
        }
        
        unique_lock(Mutex& m, std::defer_lock_t) 
            : mutex_(&m), owns_lock_(false) {
            // Don't lock
        }
        
        unique_lock(Mutex& m, std::try_to_lock_t) 
            : mutex_(&m), owns_lock_(mutex_->try_lock()) {
            // Try to lock
        }
        
        unique_lock(Mutex& m, std::adopt_lock_t) 
            : mutex_(&m), owns_lock_(true) {
            // Already locked
        }
        
        ~unique_lock() {
            if (owns_lock_) {
                mutex_->unlock();
            }
        }
        
        // Move semantics
        unique_lock(unique_lock&& other) noexcept
            : mutex_(other.mutex_), owns_lock_(other.owns_lock_) {
            other.mutex_ = nullptr;
            other.owns_lock_ = false;
        }
        
        unique_lock& operator=(unique_lock&& other) noexcept {
            if (owns_lock_) {
                mutex_->unlock();
            }
            mutex_ = other.mutex_;
            owns_lock_ = other.owns_lock_;
            other.mutex_ = nullptr;
            other.owns_lock_ = false;
            return *this;
        }
        
        void lock() {
            if (!mutex_) throw std::system_error(/*...*/);
            if (owns_lock_) throw std::system_error(/*...*/);
            mutex_->lock();
            owns_lock_ = true;
        }
        
        bool try_lock() {
            if (!mutex_) throw std::system_error(/*...*/);
            if (owns_lock_) throw std::system_error(/*...*/);
            owns_lock_ = mutex_->try_lock();
            return owns_lock_;
        }
        
        void unlock() {
            if (!owns_lock_) throw std::system_error(/*...*/);
            mutex_->unlock();
            owns_lock_ = false;
        }
        
        Mutex* mutex() const noexcept { return mutex_; }
        bool owns_lock() const noexcept { return owns_lock_; }
        explicit operator bool() const noexcept { return owns_lock_; }
        
    private:
        Mutex* mutex_;
        bool owns_lock_;
    };
}
```

### std::scoped_lock Implementation (C++17)

```cpp
// Simplified implementation using variadic templates
namespace std {
    template<class... MutexTypes>
    class scoped_lock {
    public:
        explicit scoped_lock(MutexTypes&... m) 
            : mutexes_(std::forward_as_tuple(m...)) {
            // Lock all mutexes with deadlock avoidance
            std::lock(std::get<MutexTypes&>(mutexes_)...);
            owns_lock_ = true;
        }
        
        explicit scoped_lock(std::adopt_lock_t, MutexTypes&... m)
            : mutexes_(std::forward_as_tuple(m...)), owns_lock_(true) {
            // Mutexes already locked
        }
        
        ~scoped_lock() {
            if (owns_lock_) {
                std::apply([](auto&... m) { (m.unlock(), ...); }, mutexes_);
            }
        }
        
        scoped_lock(const scoped_lock&) = delete;
        scoped_lock& operator=(const scoped_lock&) = delete;
        
    private:
        std::tuple<MutexTypes&...> mutexes_;
        bool owns_lock_;
    };
}
```

### std::shared_lock Implementation

```cpp
// Simplified implementation
namespace std {
    template<class Mutex>
    class shared_lock {
    public:
        explicit shared_lock(Mutex& m) 
            : mutex_(&m), owns_lock_(true) {
            mutex_->lock_shared();
        }
        
        shared_lock(Mutex& m, std::defer_lock_t) 
            : mutex_(&m), owns_lock_(false) {}
        
        shared_lock(Mutex& m, std::adopt_lock_t) 
            : mutex_(&m), owns_lock_(true) {}
        
        ~shared_lock() {
            if (owns_lock_) {
                mutex_->unlock_shared();
            }
        }
        
        // Move semantics similar to unique_lock
        shared_lock(shared_lock&& other) noexcept
            : mutex_(other.mutex_), owns_lock_(other.owns_lock_) {
            other.mutex_ = nullptr;
            other.owns_lock_ = false;
        }
        
        void lock() {
            if (!mutex_) throw std::system_error(/*...*/);
            if (owns_lock_) throw std::system_error(/*...*/);
            mutex_->lock_shared();
            owns_lock_ = true;
        }
        
        void unlock() {
            if (!owns_lock_) throw std::system_error(/*...*/);
            mutex_->unlock_shared();
            owns_lock_ = false;
        }
        
        Mutex* mutex() const noexcept { return mutex_; }
        bool owns_lock() const noexcept { return owns_lock_; }
        
    private:
        Mutex* mutex_;
        bool owns_lock_;
    };
}
```

### Deadlock Avoidance in std::lock

The std::lock function uses a deadlock avoidance algorithm:

```cpp
namespace std {
    template<class Mutex1, class Mutex2, class... MutexTypes>
    void lock(Mutex1& m1, Mutex2& m2, MutexTypes&... m) {
        // Try to lock in order, back off on contention
        while (true) {
            if (m1.try_lock()) {
                if (m2.try_lock()) {
                    if (try_lock_all(m...)) {
                        return;  // All locked
                    }
                    m2.unlock();
                }
                m1.unlock();
            }
            // Back off and retry
            std::this_thread::yield();
        }
    }
}
```

### RAII Pattern Benefits

Lock wrappers implement the RAII (Resource Acquisition Is Initialization) pattern:

```cpp
// Manual lock/unlock (error-prone)
void manual_lock_example() {
    mtx.lock();
    try {
        // Critical section
        if (some_condition) {
            mtx.unlock();
            return;  // Easy to forget unlock
        }
        mtx.unlock();
    } catch (...) {
        mtx.unlock();
        throw;
    }
}

// RAII wrapper (safe)
void raii_lock_example() {
    std::lock_guard<std::mutex> lock(mtx);
    // Critical section
    if (some_condition) {
        return;  // Automatic unlock
    }
    // Automatic unlock on scope exit
}
```

### Exception Safety

Lock wrappers provide strong exception safety guarantees:

```cpp
void exception_safe_operation() {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Even if an exception is thrown here,
    // the mutex is automatically unlocked
    throw std::runtime_error("Error");
    
    // Never reached, but lock is still released
}
```

### Lock Ownership Transfer

```cpp
void transfer_ownership() {
    std::unique_lock<std::mutex> lock1(mtx);
    
    // Transfer ownership to another unique_lock
    std::unique_lock<std::mutex> lock2 = std::move(lock1);
    
    // lock1 is now empty (no longer owns the lock)
    // lock2 owns the lock
}
```

### Condition Variable Integration

Lock wrappers work seamlessly with condition variables:

```cpp
#include <condition_variable>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void wait_for_condition() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    // Condition met, lock is held
}

void signal_condition() {
    std::lock_guard<std::mutex> lock(mtx);
    ready = true;
    cv.notify_one();
}
```

## Best Practices

1. Always prefer lock_guard for simple locking scenarios
2. Use unique_lock when you need flexibility (unlock early, transfer ownership)
3. Use scoped_lock for locking multiple mutexes (avoids deadlocks)
4. Use shared_lock for read-heavy workloads with shared_mutex
5. Never mix manual lock/unlock with RAII wrappers
6. Keep lock scope as small as possible
7. Use defer_lock when you need to delay locking
8. Lock wrappers are not copyable, only movable
9. Always check owns_lock() before manual operations on unique_lock
10. Use adopt_lock when mutex is already locked
