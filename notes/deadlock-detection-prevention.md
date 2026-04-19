# Deadlock Detection and Prevention: Usage and Implementation

## Quick Overview

| Strategy | Complexity | Overhead | Best For |
|----------|------------|----------|----------|
| Lock Ordering (`std::scoped_lock`) | Simple | None | General use, multiple mutexes |
| `std::lock` with Deadlock Avoidance | Simple | Low | When lock order unknown at compile time |
| Hierarchical Locking | Moderate | Low | Complex systems with layered resources |
| try_lock with Timeout | Simple | Low | Long operations, network I/O |
| Timeout-based Detection | Moderate | Moderate | Monitoring, debugging, connection pools |
| Graph-based Detection | Complex | High | Debugging tools, databases (not production apps) |

## Theoretical Background

### Coffman Conditions

Deadlock requires all four conditions. Breaking any one prevents deadlock:

1. **Mutual Exclusion** - Resources cannot be shared (inherent to mutexes)
2. **Hold and Wait** - Thread holds resources while waiting for others
3. **No Preemption** - Resources cannot be forcibly taken (inherent to mutexes)
4. **Circular Wait** - Thread A waits for B, B waits for A (primary target for prevention)

**In practice**: Conditions 1 and 3 are inherent constraints. Condition 2 is often necessary. Condition 4 (Circular Wait) is the practical target for most prevention strategies.

## Strategies

### 1. Lock Ordering with `std::scoped_lock`

**Description**: All threads acquire locks in the same consistent order. `std::scoped_lock` (C++17) locks multiple mutexes atomically.

**Pros**:
- Zero runtime overhead
- Simple to use
- Guaranteed deadlock prevention
- RAII-based (exception-safe)

**Cons**:
- Requires knowing lock order at compile time
- All code must follow the same convention
- Doesn't work for dynamic lock acquisition

**Realistic Use Cases**:
- Bank account transfers (lock both accounts in account ID order)
- Database operations (lock tables in fixed order)
- Multi-resource updates where resource set is known in advance

```cpp
#include <mutex>

std::mutex mtx1, mtx2;

void transfer() {
    std::scoped_lock lock(mtx1, mtx2);  // Locks both in consistent order
    // Critical section
}
```

---

### 2. `std::lock` with Deadlock Avoidance

**Description**: `std::lock` tries multiple lock orders internally with backoff. Order of arguments doesn't matter.

**Pros**:
- No need to know lock order in advance
- Handles dynamic lock acquisition
- Still has low overhead (just retry attempts)

**Cons**:
- Slightly higher overhead than `std::scoped_lock`
- Requires `std::unique_lock` with `std::defer_lock`
- Still requires all locks to be acquired together

**Realistic Use Cases**:
- When locks are determined at runtime
- Libraries that don't control lock ordering
- Code that needs to lock arbitrary pairs of resources

```cpp
#include <mutex>

std::mutex mtx1, mtx2;

void thread1() {
    std::unique_lock lock1(mtx1, std::defer_lock);
    std::unique_lock lock2(mtx2, std::defer_lock);
    std::lock(lock1, lock2);  // Tries both orders, avoids deadlock
}
```

---

### 3. Hierarchical Locking

**Description**: Assign levels to locks (e.g., LEVEL_DB=1, LEVEL_TABLE=2, LEVEL_ROW=3). Locks must be acquired in increasing level order. RAII-based enforcement at runtime.

**Pros**:
- Enforces ordering at runtime (catches violations during testing)
- Works well with layered architectures
- Clear documentation of lock hierarchy
- Exception-safe with RAII

**Cons**:
- Runtime overhead (stack checks)
- Requires assigning levels to all locks
- Can be complex to set up correctly
- Manual lock/unlock version is error-prone (use RAII version)

**Realistic Use Cases**:
- Database systems (database → table → row → page)
- File systems (volume → directory → file)
- GUI frameworks (window → widget → element)
- Embedded systems with clear hardware peripheral priorities

```cpp
#include <mutex>
#include <vector>

class HierarchicalLock {
    std::mutex& mtx_;
    int level_;
    static thread_local std::vector<int> lock_stack_;
    
public:
    HierarchicalLock(std::mutex& mtx, int level) : mtx_(mtx), level_(level) {
        if (!lock_stack_.empty() && lock_stack_.back() >= level)
            throw std::logic_error("Lock hierarchy violation");
        mtx_.lock();
        lock_stack_.push_back(level);
    }
    
    ~HierarchicalLock() {
        lock_stack_.pop_back();
        mtx_.unlock();
    }
};

// Usage
std::mutex db_mtx, table_mtx, row_mtx;
constexpr int LEVEL_DB = 1, LEVEL_TABLE = 2, LEVEL_ROW = 3;

void operation() {
    HierarchicalLock l1(db_mtx, LEVEL_DB);
    HierarchicalLock l2(table_mtx, LEVEL_TABLE);
    HierarchicalLock l3(row_mtx, LEVEL_ROW);
    // Critical section
}
```

---

### 4. try_lock with Timeout

**Description**: Use `std::timed_mutex` with `try_lock_for()`. If lock unavailable within timeout, back off and retry.

**Pros**:
- Simple to implement
- Prevents indefinite blocking
- Works with any number of locks
- No global coordination needed

**Cons**:
- Timeout values are heuristic (too short = false positives, too long = slow detection)
- Doesn't prevent deadlock, only prevents indefinite blocking
- Retry loops consume CPU
- Can mask real bugs if used as a "fix" for deadlocks

**Realistic Use Cases**:
- Network I/O operations (RPC calls, HTTP requests)
- Database connection pools
- Thread pool task acquisition
- Embedded system watchdogs
- GUI frameworks (prevent UI freezes)

```cpp
#include <mutex>
#include <thread>

std::timed_mutex mtx1, mtx2;

void operation() {
    while (true) {
        if (mtx1.try_lock_for(std::chrono::milliseconds(100))) {
            if (mtx2.try_lock_for(std::chrono::milliseconds(100))) {
                // Both locks acquired
                // Critical section
                mtx2.unlock();
                mtx1.unlock();
                break;
            }
            mtx1.unlock();
        }
        std::this_thread::yield();  // Let other threads run
    }
}
```

---

### 5. Timeout-based Detection

**Description**: Background thread monitors how long locks are held. Reports if held longer than maximum allowed time.

**Pros**:
- Simple to implement
- Predictable overhead
- Works in production
- Can provide detailed diagnostics

**Cons**:
- Timeout values are heuristic
- Only detects after deadlock occurs
- Doesn't prevent deadlock
- Requires registering all locks

**Realistic Use Cases**:
- Database connection pool monitoring
- Network I/O timeout detection
- Thread pool stuck task detection
- Message queue consumer monitoring
- Production system health checks

```cpp
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <chrono>

class TimeoutDeadlockDetector {
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
                              << std::chrono::duration_cast<std::chrono::milliseconds>(held_duration).count()
                              << "ms\n";
                }
            }
        }
    }
};
```

---

### 6. Graph-based Detection (Not Recommended for Production)

**Description**: Build wait-for graph (threads as nodes, waiting relationships as edges). Detect cycles using DFS.

**Pros**:
- Can detect any deadlock pattern
- Provides detailed information about deadlock
- Comprehensive

**Cons**:
- Very high runtime overhead
- Requires tracking all lock acquisitions globally
- Complex to implement correctly
- Detection is after-the-fact (deadlock already occurred)
- Rarely practical for general C++ applications

**Realistic Use Cases**:
- Debugging tools (Helgrind, ThreadSanitizer)
- Database deadlock detection
- Real-time OS internals
- **NOT for general production C++ applications**

---

## Best Practices

1. **Prefer prevention over detection** - Use lock ordering or `std::scoped_lock` when possible
2. **Keep lock holding time short** - Minimize time in critical sections
3. **Avoid nested locks** - Reduces complexity and deadlock risk
4. **Use RAII lock guards** - `std::scoped_lock`, `std::lock_guard`, `std::unique_lock`
5. **Document lock ordering** - Make conventions explicit in code comments
6. **Test under high contention** - Stress test to expose deadlocks
7. **Use detection tools during development** - Helgrind, ThreadSanitizer
8. **Consider lock-free alternatives** - For high-contention scenarios
9. **Use timeouts for I/O operations** - Network calls, database queries
10. **Hierarchical locking for complex systems** - When resources have natural levels

## When to Use Each Strategy

- **Simple 2-3 mutexes**: `std::scoped_lock`
- **Dynamic lock acquisition**: `std::lock` with `std::defer_lock`
- **Layered architecture**: Hierarchical locking
- **Network/IO operations**: try_lock with timeout
- **Production monitoring**: Timeout-based detection
- **Development/debugging**: Graph-based detection tools
