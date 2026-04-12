# Exception Handling in Multithreaded Contexts: Usage and Implementation

## Practical Usage

### Exception in Thread

```cpp
#include <thread>
#include <iostream>

void throwing_function() {
    throw std::runtime_error("Error in thread");
}

int main() {
    std::thread t([]() {
        try {
            throwing_function();
        } catch (const std::exception& e) {
            std::cerr << "Caught exception in thread: " << e.what() << "\n";
        }
    });
    
    t.join();
    return 0;
}
```

### Exception with std::promise

```cpp
#include <thread>
#include <future>
#include <iostream>

void worker(std::promise<int> prom) {
    try {
        throw std::runtime_error("Worker error");
        prom.set_value(42);
    } catch (...) {
        prom.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(worker, std::move(prom));
    
    try {
        int result = fut.get();
        std::cout << "Result: " << result << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Caught exception from thread: " << e.what() << "\n";
    }
    
    t.join();
    return 0;
}
```

### Exception with std::async

```cpp
#include <future>
#include <iostream>

int async_worker() {
    throw std::runtime_error("Async error");
    return 42;
}

int main() {
    auto fut = std::async(std::launch::async, async_worker);
    
    try {
        int result = fut.get();
        std::cout << "Result: " << result << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Caught exception from async: " << e.what() << "\n";
    }
    
    return 0;
}
```

### Thread Pool with Exception Handling

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>

class SafeThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    SafeThreadPool(size_t threads) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    try {
                        task();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in worker: " << e.what() << "\n";
                    } catch (...) {
                        std::cerr << "Unknown exception in worker\n";
                    }
                }
            });
        }
    }
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<decltype(f(args...))> {
        
        using ReturnType = decltype(f(args...));
        
        // Create a packaged_task that wraps the function with its arguments:
        // - std::bind: Binds the function f with its arguments (needed because
        //   packaged_task<ReturnType()> expects a no-args callable, but f may take args)
        // - std::packaged_task: Wraps the bound function for async execution
        // - std::make_shared: Creates a shared pointer to keep task alive
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        // Get a future that will receive the result or exception from the task
        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }
    
    ~SafeThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Realistic Example: Exception-Safe Task Runner

```cpp
#include <thread>
#include <future>
#include <vector>
#include <iostream>

class TaskRunner {
private:
    std::vector<std::future<void>> futures_;
    
public:
    template<class F>
    void run(F&& f) {
        auto promise = std::make_shared<std::promise<void>>();
        futures_.push_back(promise->get_future());
        
        // Detach thread to avoid storing thread objects - futures handle synchronization
        // WARNING: Must call wait_all() before program exit or threads may crash
        std::thread([promise, f = std::forward<F>(f)]() mutable {
            try {
                f();
                promise->set_value();
            } catch (const std::exception& e) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
    }
    
    void wait_all() {
        for (auto& fut : futures_) {
            try {
                fut.get();
            } catch (const std::exception& e) {
                std::cerr << "Task failed: " << e.what() << "\n";
            }
        }
        futures_.clear();
    }
    
    bool has_exceptions() {
        for (auto& fut : futures_) {
            try {
                fut.wait();
            } catch (...) {
                return true;
            }
        }
        return false;
    }
};
```

## Underlying Implementation

### std::current_exception

**Purpose:** Captures the currently active exception as an `exception_ptr` that can be stored and rethrown later.

**Intention:** Exceptions cannot normally cross thread boundaries. `std::current_exception` provides a mechanism to capture an exception object in one thread and transport it to another thread where it can be rethrown. This is essential for multithreaded exception handling - it allows a worker thread to capture an exception and send it to the main thread via `std::promise`.

**How it works:** It uses a compiler-specific ABI (Itanium C++ ABI on most platforms) to get a pointer to the active exception object and increment its reference count so the exception stays alive even after the catch block exits.

```cpp
namespace std {
    std::exception_ptr current_exception() noexcept {
        // Capture current exception
        try {
            throw;
        } catch (...) {
            return __cxxabiv1::__current_exception();
        }
    }
}
```

### std::exception_ptr Implementation

**Purpose:** A smart pointer-like type that holds a reference to an exception object.

**Intention:** `exception_ptr` solves the problem of storing and transporting exceptions across thread boundaries. Normal exception objects are destroyed when they leave their catch block, but `exception_ptr` uses reference counting to keep the exception alive as long as it's needed. This allows exceptions to be:
- Stored in data structures
- Passed between threads
- Rethrown at a later time

**How it works:** It wraps a raw pointer to the ABI-specific exception object and manages its lifetime using reference counting (increment on copy, decrement on destruction). When the count reaches zero, the exception object is deallocated.

```cpp
namespace std {
    class exception_ptr {
    private:
        void* ptr_;  // ABI-specific exception object pointer
        
    public:
        exception_ptr() noexcept : ptr_(nullptr) {}
        
        ~exception_ptr() {
            if (ptr_) {
                __cxxabiv1::__cxa_decrement_exception_refcount(ptr_);
            }
        }
        
        exception_ptr(const exception_ptr& other) noexcept : ptr_(other.ptr_) {
            if (ptr_) {
                __cxxabiv1::__cxa_increment_exception_refcount(ptr_);
            }
        }
        
        exception_ptr& operator=(const exception_ptr& other) noexcept {
            if (this != &other) {
                if (ptr_) {
                    __cxxabiv1::__cxa_decrement_exception_refcount(ptr_);
                }
                ptr_ = other.ptr_;
                if (ptr_) {
                    __cxxabiv1::__cxa_increment_exception_refcount(ptr_);
                }
            }
            return *this;
        }
        
        explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }
        
        friend void rethrow_exception(exception_ptr p);
    };
    
    void rethrow_exception(exception_ptr p) {
        __cxxabiv1::__cxa_rethrow_exception(p.ptr_);
    }
}
```

### std::promise Exception Handling

**Purpose:** Provides the mechanism for a thread to communicate an exception to another thread waiting on the corresponding `std::future`.

**Intention:** In multithreaded code, exceptions thrown in worker threads cannot propagate to the main thread. `std::promise` solves this by allowing a worker thread to store an exception via `set_exception()`, which the main thread can retrieve via `std::future::get()`. When `get()` is called, the exception is rethrown in the main thread's context.

**How it works:** The promise and future share a state object. When `set_exception()` is called, it stores an `exception_ptr` in the shared state and marks it as "exception state". When the future calls `get()`, it checks the state and calls `std::rethrow_exception()` if an exception was stored.

```cpp
template<typename T>
class promise {
private:
    std::shared_ptr<future_state<T>> state_;
    
public:
    void set_exception(std::exception_ptr ex) {
        state_->set_exception(ex);
    }
    
    // ...
};

template<typename T>
class future_state {
private:
    std::exception_ptr exception_;
    
public:
    void set_exception(std::exception_ptr ex) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (state_ != state::empty) {
            throw std::future_error(std::future_errc::promise_already_satisfied);
        }
        exception_ = ex;
        state_ = state::exception;
        cv_.notify_all();
    }
    
    T get() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return state_ != state::empty; });
        
        if (state_ == state::exception) {
            std::rethrow_exception(exception_);
        }
        
        // ... handle normal value
    }
};
```

### Thread Exception Propagation

**Purpose:** Explains what happens when an exception escapes a thread function without being caught.

**Intention:** C++ threads have their own exception handling context - exceptions cannot propagate from a thread to its creator. When an exception escapes a thread's top-level function, the runtime calls `std::terminate()` to abort the program. This is a safety mechanism to prevent undefined behavior from continuing execution in an inconsistent state.

**How it works:** The thread runtime wraps the user's function in a try/catch block. If an exception escapes, it calls the current terminate handler (by default `std::abort()`). You can override this with `std::set_terminate()` to log the exception or perform cleanup before aborting.

```cpp
// When a thread throws an unhandled exception:
// 1. std::terminate is called (default behavior)
// 2. Can be overridden with std::set_terminate

void custom_terminate_handler() {
    std::exception_ptr exptr = std::current_exception();
    try {
        if (exptr) {
            std::rethrow_exception(exptr);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Uncaught exception: " << ex.what() << "\n";
    }
    std::abort();
}

int main() {
    std::set_terminate(custom_terminate_handler);
    
    std::thread t([]() {
        throw std::runtime_error("Unhandled exception");
    });
    
    t.join();
    return 0;
}
```

### Exception-Safe Locking

**Purpose:** Ensures locks are properly released even when exceptions occur.

**Intention:** In multithreaded code, forgetting to release a lock can cause deadlocks. RAII (Resource Acquisition Is Initialization) guarantees that lock destructors run even when exceptions are thrown, ensuring locks are always released. This pattern is essential for exception safety in concurrent code.

**How it works:** `std::lock_guard` acquires the lock in its constructor and releases it in its destructor. If the function throws an exception, the stack unwinding calls the destructor, releasing the lock automatically.

```cpp
template<typename Lock, typename F>
auto with_lock(Lock& lock, F&& f) -> decltype(f()) {
    std::lock_guard<Lock> guard(lock);
    try {
        return f();
    } catch (...) {
        // Lock is automatically released by RAII
        throw;
    }
}
```

### Exception Aggregation

**Purpose:** Collects multiple exceptions from parallel operations into a single exception object.

**Intention:** When running tasks in parallel (e.g., with `std::async` or a thread pool), multiple tasks might throw exceptions simultaneously. Rather than losing all but the first exception, we can aggregate them and report all failures together. This is especially useful for batch processing where you want to know which operations succeeded and which failed.

**How it works:** A custom exception class stores a vector of `exception_ptr` objects. Each parallel task catches its exception and adds it to the aggregate. After all tasks complete, the aggregate is thrown if any exceptions occurred, allowing the caller to inspect all failures.

```cpp
class ExceptionList : public std::exception {
private:
    std::vector<std::exception_ptr> exceptions_;
    
public:
    void add(std::exception_ptr ex) {
        exceptions_.push_back(ex);
    }
    
    const char* what() const noexcept override {
        return "Multiple exceptions occurred";
    }
    
    const std::vector<std::exception_ptr>& exceptions() const {
        return exceptions_;
    }
};

template<typename It>
void parallel_execute(It first, It last) {
    ExceptionList exceptions;
    
    std::vector<std::future<void>> futures;
    for (auto it = first; it != last; ++it) {
        futures.push_back(std::async(std::launch::async, [it, &exceptions]() {
            try {
                (*it)();
            } catch (...) {
                exceptions.add(std::current_exception());
            }
        }));
    }
    
    for (auto& fut : futures) {
        fut.wait();
    }
    
    if (!exceptions.exceptions().empty()) {
        throw exceptions;
    }
}
```

### Exception in Condition Variable Wait

**Purpose:** Allows a waiting thread to be notified of an error condition instead of just normal completion.

**Intention:** Standard condition variables only signal normal conditions, but sometimes a producer needs to signal an error to waiting consumers. This pattern extends condition variables to support error propagation, allowing a producer to notify consumers that an error occurred and the operation should be aborted.

**How it works:** The condition variable stores both a normal condition flag and an `exception_ptr`. When `notify_error()` is called, it sets the exception and notifies all waiters. The wait predicate checks both conditions, and if an exception is set, it rethrows it in the waiting thread.

```cpp
#include <mutex>
#include <condition_variable>

class SafeConditionVariable {
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool condition_;
    std::exception_ptr exception_;
    
public:
    void wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return condition_ || exception_; });
        
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
    
    void notify() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            condition_ = true;
        }
        cv_.notify_one();
    }
    
    void notify_error(std::exception_ptr ex) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            exception_ = ex;
        }
        cv_.notify_all();
    }
};
```

### Coroutine Exception Handling

**Purpose:** Captures and propagates exceptions that occur in C++20 coroutines.

**Intention:** Coroutines can suspend and resume across different call stacks, making exception handling more complex. The promise type in a coroutine handles exceptions by storing them in an `exception_ptr` when they escape the coroutine body. When the coroutine is resumed or awaited, the exception is rethrown in the caller's context.

**How it works:** The compiler generates a promise object for each coroutine. If an exception escapes the coroutine body, the promise's `unhandled_exception()` method is called, which captures the exception. When the caller awaits the coroutine or co_await returns, the promise checks for stored exceptions and rethrows them.

```cpp
struct promise_type {
    std::exception_ptr exception_;
    
    void unhandled_exception() {
        exception_ = std::current_exception();
    }
    
    // When coroutine is resumed, check for exception
    void return_void() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }
};
```

### Exception Safety Levels

**Purpose:** Defines different levels of guarantees that operations provide when exceptions occur.

**Intention:** Exception safety is about the state of an object or program after an exception is thrown. These levels help users understand what they can rely on when operations fail. In multithreaded contexts, strong exception safety is particularly important to avoid leaving shared data in inconsistent states.

**Levels:**
- **Basic guarantee:** No resource leaks, object remains in a valid (but unspecified) state
- **Strong guarantee:** Operation either succeeds completely or has no effect (transactional semantics)
- **No-throw guarantee:** Operation never throws exceptions (critical for destructors and move operations)

```cpp
// Basic guarantee: No resource leaks, object valid but unspecified state
void basic_guarantee_function() {
    Resource r;
    try {
        // May throw
        r.do_something();
    } catch (...) {
        // Clean up
        throw;
    }
}

// Strong guarantee: Operation either succeeds completely or has no effect
void strong_guarantee_function() {
    Resource r;
    auto backup = r.backup();  // Copy current state
    
    try {
        r.do_something();
    } catch (...) {
        r.restore(backup);  // Restore original state
        throw;
    }
}

// No-throw guarantee: Operation never throws
void nothrow_function() noexcept {
    // No exceptions thrown
}
```

### Exception in Destructor

**Purpose:** Ensures destructors don't throw exceptions, which would cause program termination during stack unwinding.

**Intention:** If an exception is already being handled and a destructor throws another exception, the program calls `std::terminate()` immediately. This is because C++ cannot handle two simultaneous exceptions. Destructors must be `noexcept` (or catch all exceptions internally) to prevent this scenario.

**How it works:** Mark destructors as `noexcept` and catch any exceptions that might occur during cleanup. Log errors but don't rethrow. If cleanup absolutely cannot proceed without throwing, use `std::terminate()` explicitly rather than letting the exception escape.

```cpp
class SafeDestructor {
private:
    Resource* resource_;
    
public:
    ~SafeDestructor() noexcept {
        try {
            delete resource_;
        } catch (...) {
            // Log error but don't throw from destructor
            std::cerr << "Exception in destructor ignored\n";
        }
    }
};
```

## Best Practices

1. Always catch exceptions in thread entry points
2. Use std::promise/set_exception for thread-to-main communication
3. Use std::async for automatic exception propagation
4. Implement exception handling in thread pool workers
5. Use RAII for exception-safe resource management
6. Be aware of std::terminate for unhandled thread exceptions
6. Aggregate exceptions from multiple threads
7. Provide noexcept guarantees when appropriate
8. Don't throw from destructors
9. Use exception_ptr to transport exceptions across threads
10. Document exception safety guarantees clearly
