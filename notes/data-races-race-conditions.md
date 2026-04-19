# Data Races vs Race Conditions: Usage and Implementation

## Practical Usage

### Data Race Example - Undefined Behavior

```cpp
#include <thread>
#include <iostream>

int counter = 0;  // Shared data without synchronization

void increment() {
    ++counter;  // DATA RACE: Unsynchronized access
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter << "\n";  // Undefined result
    return 0;
}
```

### Fixing Data Race with Atomic

```cpp
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> counter(0);  // Atomic prevents data race

void increment() {
    counter.fetch_add(1, std::memory_order_relaxed);
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter.load() << "\n";  // Always 2
    return 0;
}
```

### Race Condition Example - Logical Error (Check-then-act)

```cpp
#include <mutex>
#include <thread>
#include <iostream>

std::mutex mtx;
int balance = 100;

void withdraw(int amount) {
    // RACE CONDITION: Check without lock
    if (balance >= amount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // Act with lock - but check and act are not atomic
        std::lock_guard<std::mutex> lock(mtx);
        balance -= amount;
    }
}

int main() {
    std::thread t1(withdraw, 60);
    std::thread t2(withdraw, 60);
    
    t1.join();
    t2.join();
    
    std::cout << "Balance: " << balance << "\n";  // Could be -20 (negative!)
    return 0;
}
```

### Fixing Race Condition with Proper Synchronization

```cpp
#include <mutex>
#include <thread>
#include <iostream>

std::mutex mtx;
int balance = 100;

void withdraw(int amount) {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (balance >= amount) {
        balance -= amount;  // Atomic check-and-act
    }
}

int main() {
    std::thread t1(withdraw, 60);
    std::thread t2(withdraw, 60);
    
    t1.join();
    t2.join();
    
    std::cout << "Balance: " << balance << "\n";  // Always 40
    return 0;
}
```

### Double-Checked Locking Pattern

**Intuition**: Optimize lazy initialization by avoiding lock acquisition after the singleton is created. First check (no lock) returns immediately if instance exists, avoiding lock overhead. Second check (with lock) handles race where multiple threads pass first check simultaneously.

**Note**: Original pattern has memory ordering issues in pre-C++11. In modern C++, prefer static local variables (thread-safe in C++11), `std::call_once`, or `std::atomic` with proper memory ordering.

```cpp
#include <mutex>
#include <memory>

class Singleton {
private:
    static std::shared_ptr<Singleton> instance_;
    static std::mutex mtx_;
    
    Singleton() {}
    
public:
    static std::shared_ptr<Singleton> get_instance() {
        if (!instance_) {  // First check (no lock)
            std::lock_guard<std::mutex> lock(mtx_);
            if (!instance_) {  // Second check (with lock)
                instance_ = std::shared_ptr<Singleton>(new Singleton());
            }
        }
        return instance_;
    }
};

std::shared_ptr<Singleton> Singleton::instance_;
std::mutex Singleton::mtx_;
```

### Realistic Example: Bank Account Transfer

```cpp
#include <mutex>
#include <iostream>

class Account {
private:
    mutable std::mutex mtx_;
    int balance_;
    
public:
    Account(int balance) : balance_(balance) {}
    
    void deposit(int amount) {
        std::lock_guard<std::mutex> lock(mtx_);
        balance_ += amount;
    }
    
    bool withdraw(int amount) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (balance_ >= amount) {
            balance_ -= amount;
            return true;
        }
        return false;
    }
    
    int balance() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return balance_;
    }
};

void transfer(Account& from, Account& to, int amount) {
    // Lock both accounts to prevent race condition
    std::scoped_lock lock(from.mtx_, to.mtx_);
    
    if (from.withdraw(amount)) {
        to.deposit(amount);
    }
}
```

## Underlying Implementation

### Data Race Definition

A data race occurs when:

1. Two or more threads access the same memory location
2. At least one access is a write
3. No synchronization establishes a happens-before relationship

```cpp
// Data race example
int x = 0;

void thread1() {
    x = 1;  // Write
}

void thread2() {
    int y = x;  // Read
}

// No happens-before between thread1 and thread2
// This is a data race = UNDEFINED BEHAVIOR
```

### Race Condition Definition

A race condition is a logical error where the outcome depends on the timing of thread execution:

```cpp
// Race condition example (with proper synchronization)
std::mutex mtx;
int x = 0;

void thread1() {
    std::lock_guard<std::mutex> lock(mtx);
    if (x == 0) {  // Check
        x = 1;      // Act
    }
}

void thread2() {
    std::lock_guard<std::mutex> lock(mtx);
    if (x == 0) {  // Check
        x = 1;      // Act
    }
}

// No data race (properly synchronized)
// But race condition: only one thread should succeed
```

### Memory Model and Data Races

The C++ memory model defines data race behavior:

```cpp
// Data race = Undefined Behavior
// Compiler can assume no data race and optimize aggressively

int x = 0;
bool flag = false;

void thread1() {
    x = 42;
    flag = true;  // Compiler might reorder these
}

void thread2() {
    if (flag) {
        // Without synchronization, might not see x = 42
        // This is a data race = UB
        std::cout << x << "\n";
    }
}
```

### Happens-Before Relationship

```cpp
// Establishing happens-before
std::atomic<int> x(0);
std::atomic<bool> ready(false);

void thread1() {
    x.store(42, std::memory_order_relaxed);  // A
    ready.store(true, std::memory_order_release);  // B
}

void thread2() {
    if (ready.load(std::memory_order_acquire)) {  // C
        // B synchronizes-with C
        // A happens-before C
        int y = x.load(std::memory_order_relaxed);  // D
        // y must be 42 (no data race)
    }
}
```

### Sequential Consistency

```cpp
// Sequential consistency prevents data races
std::atomic<int> x(0);
std::atomic<int> y(0);

void thread1() {
    x.store(1, std::memory_order_seq_cst);  // Default
}

void thread2() {
    y.store(1, std::memory_order_seq_cst);  // Default
}

void thread3() {
    int a = x.load(std::memory_order_seq_cst);
    int b = y.load(std::memory_order_seq_cst);
    // All threads agree on global order
}
```

### Compiler Optimizations and Data Races

```cpp
// Without data race, compiler can optimize:
int x = 0;
int y = 0;

void thread1() {
    x = 1;
    y = 1;
}

// Compiler might store to x and y in any order
// Or even combine them if it proves no data race

// With data race, these optimizations are invalid
// But compiler assumes no data race anyway
```

### Detection Tools

```cpp
// ThreadSanitizer (TSan) detects data races
// Compile with: -fsanitize=thread -g

// Example TSan output:
// WARNING: ThreadSanitizer: data race on...
//   Write of size 4 at...
//   Previous write of size 4 at...
//   Location is heap block of size...
```

### Data Race Prevention Patterns

```cpp
// Pattern 1: Mutex protection
class ThreadSafeCounter {
private:
    mutable std::mutex mtx_;
    int count_;
    
public:
    void increment() {
        std::lock_guard<std::mutex> lock(mtx_);
        ++count_;
    }
    
    int get() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }
};

// Pattern 2: Atomic operations
class AtomicCounter {
private:
    std::atomic<int> count_;
    
public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return count_.load(std::memory_order_relaxed);
    }
};

// Pattern 3: Thread-local storage
class ThreadLocalCounter {
private:
    thread_local int count_;
    
public:
    void increment() {
        ++count_;
    }
    
    int get() const {
        return count_;
    }
};
```

### Race Condition Patterns

```cpp
// Pattern 1: Check-then-act (TOCTOU)
if (file_exists(filename)) {  // Check
    open_file(filename);      // Act (might fail)
}

// Pattern 2: Read-modify-write
temp = shared_value;     // Read
temp = process(temp);    // Modify
shared_value = temp;      // Write

// Pattern 3: Publish-subscribe
publisher.set_value(42);  // Publish
int value = subscriber.get_value();  // Subscribe
```

### Solving Race Conditions

```cpp
// Solution 1: Atomic check-and-set
std::atomic<int> value(0);
bool expected = 0;
value.compare_exchange_strong(expected, 42);  // Atomic

// Solution 2: Mutex for critical section
std::mutex mtx;
int value = 0;

{
    std::lock_guard<std::mutex> lock(mtx);
    if (value == 0) {
        value = 42;
    }
}

// Solution 3: Condition variable for state changes
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void wait_for_ready() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
}
```

### Memory Ordering and Race Conditions

```cpp
// Even with atomics, wrong ordering causes race conditions
std::atomic<int> data(0);
std::atomic<bool> initialized(false);

void producer() {
    data.store(42, std::memory_order_relaxed);  // A
    initialized.store(true, std::memory_order_relaxed);  // B
}

void consumer() {
    if (initialized.load(std::memory_order_relaxed)) {  // C
        // Might not see data = 42 due to reordering
        int value = data.load(std::memory_order_relaxed);  // D
    }
}

// Fix: Use acquire-release semantics
void producer_fixed() {
    data.store(42, std::memory_order_relaxed);
    initialized.store(true, std::memory_order_release);  // Release
}

void consumer_fixed() {
    if (initialized.load(std::memory_order_acquire)) {  // Acquire
        int value = data.load(std::memory_order_relaxed);
        // Guaranteed to see data = 42
    }
}
```

### Static Initialization Race

```cpp
// Static local initialization is thread-safe (C++11)
Singleton& get_instance() {
    static Singleton instance;  // Thread-safe initialization
    return instance;
}

// But be careful with other static initialization
int& get_global() {
    static int* value = new int(42);  // Thread-safe
    return *value;
}

// Multiple static initialization can still race
int& global1() {
    static int x = 42;  // Thread-safe
    return x;
}

int& global2() {
    static int y = global1();  // Potential deadlock if called concurrently
    return y;
}
```

## Best Practices

1. Always use synchronization for shared mutable data
2. Prefer atomic operations for simple shared variables
3. Use mutexes for complex critical sections
4. Be aware of check-then-act race conditions
5. Use ThreadSanitizer to detect data races during testing
6. Design for immutability when possible
7. Use thread-local storage to avoid sharing
8. Understand the difference between data races and race conditions
9. Use proper memory ordering for atomic operations
10. Document synchronization requirements clearly
