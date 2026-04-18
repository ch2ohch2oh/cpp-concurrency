# Deadlock Detection and Prevention: Usage and Implementation

## Practical Usage

### Deadlock Example - Circular Wait

```cpp
#include <mutex>
#include <thread>
#include <iostream>

std::mutex mtx1;
std::mutex mtx2;

void thread1() {
    std::lock_guard<std::mutex> lock1(mtx1);
    std::cout << "Thread 1: Locked mtx1\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::lock_guard<std::mutex> lock2(mtx2);  // DEADLOCK if thread2 has mtx2
    std::cout << "Thread 1: Locked mtx2\n";
}

void thread2() {
    std::lock_guard<std::mutex> lock2(mtx2);
    std::cout << "Thread 2: Locked mtx2\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::lock_guard<std::mutex> lock1(mtx1);  // DEADLOCK if thread1 has mtx1
    std::cout << "Thread 2: Locked mtx1\n";
}

int main() {
    std::thread t1(thread1);
    std::thread t2(thread2);
    
    t1.join();
    t2.join();  // Never returns if deadlock occurs
    
    return 0;
}
```

### Prevention Strategy 1: Lock Ordering

```cpp
#include <mutex>
#include <thread>

std::mutex mtx1;
std::mutex mtx2;

// Always lock in the same order
void safe_thread1() {
    std::scoped_lock lock(mtx1, mtx2);  // Locks both in order
    // Critical section
}

void safe_thread2() {
    std::scoped_lock lock(mtx1, mtx2);  // Same order
    // Critical section
}
```

### Prevention Strategy 2: std::lock with Deadlock Avoidance

```cpp
#include <mutex>
#include <thread>

std::mutex mtx1;
std::mutex mtx2;

void thread1() {
    std::unique_lock<std::mutex> lock1(mtx1, std::defer_lock);
    std::unique_lock<std::mutex> lock2(mtx2, std::defer_lock);
    
    // Lock both with deadlock avoidance
    std::lock(lock1, lock2);
    // Critical section
}

void thread2() {
    std::unique_lock<std::mutex> lock2(mtx2, std::defer_lock);
    std::unique_lock<std::mutex> lock1(mtx1, std::defer_lock);
    
    // Order doesn't matter with std::lock
    std::lock(lock2, lock1);
    // Critical section
}
```

### Prevention Strategy 3: try_lock with Timeout

```cpp
#include <mutex>
#include <thread>

std::timed_mutex mtx1;
std::timed_mutex mtx2;

void thread1() {
    while (true) {
        if (mtx1.try_lock_for(std::chrono::milliseconds(100))) {
            if (mtx2.try_lock_for(std::chrono::milliseconds(100))) {
                // Both locks acquired
                mtx2.unlock();
                mtx1.unlock();
                break;
            }
            mtx1.unlock();
        }
        std::this_thread::yield();
    }
}
```

### Prevention Strategy 4: Hierarchical Locking

```cpp
#include <mutex>
#include <thread>

class LockHierarchy {
public:
    static constexpr int LEVEL_A = 1;
    static constexpr int LEVEL_B = 2;
    static constexpr int LEVEL_C = 3;
    
    static void lock(std::mutex& mtx, int level) {
        if (current_level_ >= level) {
            throw std::runtime_error("Lock hierarchy violation");
        }
        mtx.lock();
        current_level_ = level;
    }
    
    static void unlock(std::mutex& mtx, int level) {
        mtx.unlock();
        current_level_ = level - 1;
    }
    
private:
    static thread_local int current_level_;
};

thread_local int LockHierarchy::current_level_ = 0;
```

### Deadlock Detection with Timeout

```cpp
#include <mutex>
#include <chrono>
#include <iostream>

class DeadlockDetector {
private:
    std::mutex mtx_;
    std::chrono::milliseconds timeout_;
    
public:
    DeadlockDetector(std::chrono::milliseconds timeout) : timeout_(timeout) {}
    
    bool try_lock_with_detection(std::timed_mutex& mtx, const char* name) {
        auto start = std::chrono::steady_clock::now();
        
        while (true) {
            if (mtx.try_lock_for(std::chrono::milliseconds(100))) {
                return true;
            }
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > timeout_) {
                std::cerr << "Potential deadlock detected on " << name << "\n";
                return false;
            }
        }
    }
};
```

### Realistic Example: Bank Account Transfer

```cpp
#include <mutex>
#include <iostream>

class Account {
private:
    std::mutex mtx_;
    int balance_;
    
public:
    Account(int balance) : balance_(balance) {}
    
    void deposit(int amount) {
        std::lock_guard<std::mutex> lock(mtx_);
        balance_ += amount;
    }
    
    void withdraw(int amount) {
        std::lock_guard<std::mutex> lock(mtx_);
        balance_ -= amount;
    }
    
    int balance() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return balance_;
    }
    
    std::mutex& mutex() { return mtx_; }
};

// Safe transfer using std::lock
void transfer(Account& from, Account& to, int amount) {
    std::scoped_lock lock(from.mutex(), to.mutex());
    
    from.withdraw(amount);
    to.deposit(amount);
}
```

## Underlying Implementation

### Deadlock Conditions (Coffman Conditions)

Deadlock requires all four conditions:

1. **Mutual Exclusion**: Resources cannot be shared
2. **Hold and Wait**: Thread holds resources while waiting for others
3. **No Preemption**: Resources cannot be forcibly taken
4. **Circular Wait**: Thread A waits for B, B waits for A, etc.

### Resource Allocation Graph

```
Thread 1 --> Mutex A
Thread 1 --> Mutex B (waiting)
Thread 2 --> Mutex B
Thread 2 --> Mutex A (waiting)

Cycle in graph = Deadlock
```

### std::lock Implementation with Deadlock Avoidance

```cpp
// Simplified implementation
namespace std {
    template<class Mutex1, class Mutex2>
    void lock(Mutex1& m1, Mutex2& m2) {
        while (true) {
            // Try to lock in order
            if (m1.try_lock()) {
                if (m2.try_lock()) {
                    return;  // Both locked
                }
                m1.unlock();  // Back off
            }
            
            // Try opposite order
            if (m2.try_lock()) {
                if (m1.try_lock()) {
                    return;  // Both locked
                }
                m2.unlock();  // Back off
            }
            
            // Back off and retry
            std::this_thread::yield();
        }
    }
}
```

### Hierarchical Lock Implementation

```cpp
class HierarchicalLock {
private:
    std::mutex& mtx_;
    int level_;
    static thread_local std::vector<int> lock_stack_;
    
public:
    HierarchicalLock(std::mutex& mtx, int level) : mtx_(mtx), level_(level) {
        if (!lock_stack_.empty() && lock_stack_.back() >= level) {
            throw std::logic_error("Lock hierarchy violation");
        }
        mtx_.lock();
        lock_stack_.push_back(level);
    }
    
    ~HierarchicalLock() {
        lock_stack_.pop_back();
        mtx_.unlock();
    }
};

thread_local std::vector<int> HierarchicalLock::lock_stack_;
```

### Deadlock Detection Algorithm

```cpp
class DeadlockDetector {
private:
    struct LockInfo {
        std::thread::id owner;
        std::chrono::steady_clock::time_point acquired_at;
    };
    
    std::unordered_map<void*, LockInfo> lock_map_;
    std::mutex detector_mtx_;
    
public:
    void acquire_lock(void* lock_addr) {
        std::lock_guard<std::mutex> lock(detector_mtx_);
        
        auto thread_id = std::this_thread::get_id();
        auto now = std::chrono::steady_clock::now();
        
        lock_map_[lock_addr] = {thread_id, now};
        
        // Check for cycles
        if (has_cycle()) {
            std::cerr << "Deadlock detected!\n";
        }
    }
    
    void release_lock(void* lock_addr) {
        std::lock_guard<std::mutex> lock(detector_mtx_);
        lock_map_.erase(lock_addr);
    }
    
private:
    bool has_cycle() {
        // Build wait graph and check for cycles
        // Using DFS or Tarjan's algorithm
        return false;  // Simplified
    }
};
```

### Wait Graph Construction

```cpp
struct WaitGraph {
    struct Node {
        std::thread::id thread_id;
        std::vector<void*> holds;
        std::vector<void*> waits_for;
    };
    
    std::unordered_map<std::thread::id, Node> nodes;
    
    void add_edge(std::thread::id from, void* to) {
        nodes[from].waits_for.push_back(to);
    }
    
    bool has_cycle() {
        // DFS to detect cycles
        std::set<std::thread::id> visited;
        std::set<std::thread::id> rec_stack;
        
        for (const auto& [id, node] : nodes) {
            if (dfs(id, visited, rec_stack)) {
                return true;
            }
        }
        return false;
    }
    
private:
    bool dfs(std::thread::id id, std::set<std::thread::id>& visited,
             std::set<std::thread::id>& rec_stack) {
        if (rec_stack.count(id)) return true;
        if (visited.count(id)) return false;
        
        visited.insert(id);
        rec_stack.insert(id);
        
        for (void* lock_ptr : nodes[id].waits_for) {
            // Find thread holding this lock
            for (const auto& [tid, node] : nodes) {
                for (void* held : node.holds) {
                    if (held == lock_ptr) {
                        if (dfs(tid, visited, rec_stack)) {
                            return true;
                        }
                    }
                }
            }
        }
        
        rec_stack.erase(id);
        return false;
    }
};
```

### Timeout-Based Detection

```cpp
class TimeoutDeadlockDetector {
private:
    struct LockState {
        std::thread::id owner;
        std::chrono::steady_clock::time_point acquired_at;
        std::chrono::milliseconds max_hold_time;
    };
    
    std::unordered_map<void*, LockState> lock_states_;
    std::mutex mtx_;
    std::thread monitor_thread_;
    std::atomic<bool> running_;
    
public:
    TimeoutDeadlockDetector() : running_(true) {
        monitor_thread_ = std::thread([this] { monitor(); });
    }
    
    ~TimeoutDeadlockDetector() {
        running_ = false;
        monitor_thread_.join();
    }
    
    void register_lock(void* lock_ptr, std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mtx_);
        lock_states_[lock_ptr] = {
            std::this_thread::get_id(),
            std::chrono::steady_clock::now(),
            timeout
        };
    }
    
    void unregister_lock(void* lock_ptr) {
        std::lock_guard<std::mutex> lock(mtx_);
        lock_states_.erase(lock_ptr);
    }
    
private:
    void monitor() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::lock_guard<std::mutex> lock(mtx_);
            auto now = std::chrono::steady_clock::now();
            
            for (const auto& [lock_ptr, state] : lock_states_) {
                auto held_duration = now - state.acquired_at;
                if (held_duration > state.max_hold_time) {
                    std::cerr << "Potential deadlock: lock held for "
                              << std::chrono::duration_cast<
                                  std::chrono::milliseconds>(held_duration).count()
                              << "ms by thread " << state.owner << "\n";
                }
            }
        }
    }
};
```

### Lock Ordering Enforcement

```cpp
class OrderedLockManager {
private:
    static std::atomic<uint64_t> next_order_;
    static std::unordered_map<std::mutex*, uint64_t> lock_order_;
    static std::mutex order_mtx_;
    
public:
    static void register_lock(std::mutex* mtx) {
        std::lock_guard<std::mutex> lock(order_mtx_);
        lock_order_[mtx] = next_order_++;
    }
    
    static void verify_order(std::mutex* mtx) {
        std::lock_guard<std::mutex> lock(order_mtx_);
        uint64_t current_order = lock_order_[mtx];
        
        // Check if any held lock has higher order
        for (const auto& [held_mtx, order] : lock_order_) {
            if (held_mtx->native_handle() == /* currently held */) {
                if (order > current_order) {
                    throw std::runtime_error("Lock order violation");
                }
            }
        }
    }
};

std::atomic<uint64_t> OrderedLockManager::next_order_{0};
std::unordered_map<std::mutex*, uint64_t> OrderedLockManager::lock_order_;
std::mutex OrderedLockManager::order_mtx_;
```

### RAII Wrapper with Deadlock Prevention

```cpp
template<typename Mutex>
class DeadlockSafeLock {
private:
    Mutex* mtx_;
    bool owns_;
    
public:
    DeadlockSafeLock(Mutex& mtx) : mtx_(&mtx), owns_(false) {
        // Implement timeout-based acquisition
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        
        while (std::chrono::steady_clock::now() < deadline) {
            if (mtx_->try_lock()) {
                owns_ = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        throw std::runtime_error("Failed to acquire lock - potential deadlock");
    }
    
    ~DeadlockSafeLock() {
        if (owns_) {
            mtx_->unlock();
        }
    }
};
```

## Best Practices

1. Always lock mutexes in a consistent order
2. Use std::lock or std::scoped_lock for multiple mutexes
3. Keep lock holding time as short as possible
4. Avoid nested locks when possible
5. Use try_lock with timeouts for long operations
6. Implement hierarchical locking for complex systems
7. Use deadlock detection tools (Helgrind, ThreadSanitizer)
8. Design lock-free alternatives when possible
9. Document lock ordering requirements clearly
10. Test under high contention to expose deadlocks
