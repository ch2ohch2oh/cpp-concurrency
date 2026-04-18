# Futures and Promises: Usage and Implementation

## Practical Usage

### std::future and std::promise - Basic Usage

```cpp
#include <future>
#include <thread>
#include <iostream>

void worker(std::promise<int> p) {
    try {
        // Do some work
        int result = 42;
        p.set_value(result);
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(worker, std::move(prom));
    
    // Wait for result
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    t.join();
    return 0;
}
```

### std::async - Asynchronous Task Execution

```cpp
#include <future>
#include <iostream>

int calculate() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 42;
}

int main() {
    // Launch async task
    std::future<int> fut = std::async(std::launch::async, calculate);
    
    // Do other work while task runs
    std::cout << "Task is running...\n";
    
    // Get result (blocks if not ready)
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    return 0;
}
```

### std::packaged_task - Task with Future

```cpp
#include <future>
#include <functional>
#include <iostream>

int square(int x) {
    return x * x;
}

int main() {
    std::packaged_task<int(int)> task(square);
    std::future<int> fut = task.get_future();
    
    std::thread t(std::move(task), 10);
    
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    t.join();
    return 0;
}
```

### Multiple Futures with std::when_all (C++23) or Manual Implementation

```cpp
#include <future>
#include <vector>
#include <iostream>

int main() {
    std::vector<std::future<int>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * i));
            return i * i;
        }));
    }
    
    // Wait for all futures
    for (auto& fut : futures) {
        std::cout << "Result: " << fut.get() << "\n";
    }
    
    return 0;
}
```

### std::shared_future - Multiple Readers

```cpp
#include <future>
#include <thread>
#include <iostream>

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    std::shared_future<int> shared = fut.share();
    
    std::thread t1([shared] {
        std::cout << "Thread 1: " << shared.get() << "\n";
    });
    
    std::thread t2([shared] {
        std::cout << "Thread 2: " << shared.get() << "\n";
    });
    
    prom.set_value(42);
    
    t1.join();
    t2.join();
    
    return 0;
}
```

### Timeout with future::wait_for

```cpp
#include <future>
#include <chrono>
#include <iostream>

int main() {
    std::future<int> fut = std::async(std::launch::async, [] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 42;
    });
    
    if (fut.wait_for(std::chrono::seconds(1)) == std::future_status::ready) {
        std::cout << "Result: " << fut.get() << "\n";
    } else {
        std::cout << "Timeout\n";
    }
    
    return 0;
}
```

### Realistic Example: Parallel MapReduce

```cpp
#include <future>
#include <vector>
#include <algorithm>
#include <numeric>

template<typename Iterator, typename Func>
auto parallel_map(Iterator begin, Iterator end, Func f) 
    -> std::vector<decltype(f(*begin))> {
    
    using ResultType = decltype(f(*begin));
    std::vector<std::future<ResultType>> futures;
    
    for (auto it = begin; it != end; ++it) {
        futures.push_back(std::async(std::launch::async, [f, it] {
            return f(*it);
        }));
    }
    
    std::vector<ResultType> results;
    for (auto& fut : futures) {
        results.push_back(fut.get());
    }
    
    return results;
}

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto results = parallel_map(data.begin(), data.end(), [](int x) {
        return x * x;
    });
    
    int sum = std::accumulate(results.begin(), results.end(), 0);
    std::cout << "Sum of squares: " << sum << "\n";
    
    return 0;
}
```

## Underlying Implementation

### Future/Promise Architecture

The future/promise pattern implements a one-time channel:

```
Producer Thread          Consumer Thread
    |                         |
    v                         v
[Promise] -----> [Shared State] <----- [Future]
    |                         |
    v                         v
Set value/exception      Wait for result
```

### Shared State Implementation

```cpp
// Simplified conceptual implementation
namespace std {
    template<typename T>
    class future_state {
    public:
        enum class state { empty, value, exception };
        
        void set_value(const T& value) {
            std::lock_guard<std::mutex> lock(mtx_);
            if (state_ != state::empty) {
                throw std::future_error(std::future_errc::promise_already_satisfied);
            }
            value_ = value;
            state_ = state::value;
            cv_.notify_all();
        }
        
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
            cv_.wait(lock, [this] { 
                return state_ != state::empty; 
            });
            
            if (state_ == state::exception) {
                std::rethrow_exception(exception_);
            }
            
            state_ = state::empty;  // Consume the value
            return std::move(value_);
        }
        
        bool is_ready() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return state_ != state::empty;
        }
        
    private:
        mutable std::mutex mtx_;
        std::condition_variable cv_;
        state state_ = state::empty;
        T value_;
        std::exception_ptr exception_;
    };
}
```

### std::promise Implementation

```cpp
template<typename T>
class promise {
public:
    promise() : state_(std::make_shared<future_state<T>>()) {}
    
    std::future<T> get_future() {
        if (retrieved_) {
            throw std::future_error(std::future_errc::future_already_retrieved);
        }
        retrieved_ = true;
        return std::future<T>(state_);
    }
    
    void set_value(const T& value) {
        state_->set_value(value);
    }
    
    void set_value(T&& value) {
        state_->set_value(std::move(value));
    }
    
    void set_exception(std::exception_ptr ex) {
        state_->set_exception(ex);
    }
    
private:
    std::shared_ptr<future_state<T>> state_;
    bool retrieved_ = false;
};
```

### std::future Implementation

```cpp
template<typename T>
class future {
public:
    future() = default;
    future(std::shared_ptr<future_state<T>> state) 
        : state_(std::move(state)) {}
    
    T get() {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        return state_->get();
    }
    
    bool valid() const noexcept {
        return state_ != nullptr;
    }
    
    bool is_ready() const {
        if (!state_) return false;
        return state_->is_ready();
    }
    
    void wait() const {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->wait();
    }
    
private:
    std::shared_ptr<future_state<T>> state_;
};
```

### std::async Implementation

```cpp
// Simplified implementation
template<typename Function, typename... Args>
auto async(Function&& f, Args&&... args) 
    -> std::future<decltype(f(args...))> {
    
    using ResultType = decltype(f(args...));
    std::promise<ResultType> prom;
    std::future<ResultType> fut = prom.get_future();
    
    std::thread t([prom = std::move(prom), f, args...]() mutable {
        try {
            prom.set_value(f(args...));
        } catch (...) {
            prom.set_exception(std::current_exception());
        }
    });
    
    t.detach();  // The future keeps the shared state alive
    
    return fut;
}
```

### std::packaged_task Implementation

```cpp
template<typename Signature>
class packaged_task;

template<typename R, typename... Args>
class packaged_task<R(Args...)> {
public:
    packaged_task() = default;
    
    template<typename F>
    explicit packaged_task(F&& f) : func_(std::forward<F>(f)) {}
    
    std::future<R> get_future() {
        if (retrieved_) {
            throw std::future_error(std::future_errc::future_already_retrieved);
        }
        retrieved_ = true;
        return prom_.get_future();
    }
    
    void operator()(Args... args) {
        try {
            prom_.set_value(func_(args...));
        } catch (...) {
            prom_.set_exception(std::current_exception());
        }
    }
    
private:
    std::function<R(Args...)> func_;
    std::promise<R> prom_;
    bool retrieved_ = false;
};
```

### std::shared_future Implementation

```cpp
template<typename T>
class shared_future {
public:
    shared_future() = default;
    shared_future(std::future<T>&& fut) 
        : state_(std::move(fut.state_)) {}
    
    T get() const {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        return state_->get();  // Note: doesn't consume the value
    }
    
    bool valid() const noexcept {
        return state_ != nullptr;
    }
    
    void wait() const {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->wait();
    }
    
private:
    std::shared_ptr<future_state<T>> state_;
};
```

### Launch Policies

```cpp
enum class launch {
    async = 1,      // Run in a new thread
    deferred = 2,   // Run when get() is called
    any = async | deferred
};

// Implementation of deferred execution
template<typename Function, typename... Args>
auto async_deferred(Function&& f, Args&&... args) {
    using ResultType = decltype(f(args...));
    std::promise<ResultType> prom;
    std::future<ResultType> fut = prom.get_future();
    
    // Store the task for later execution
    // When get() is called, execute the task
    auto state = fut.state_;
    state->set_deferred_task([prom = std::move(prom), f, args...]() mutable {
        try {
            prom.set_value(f(args...));
        } catch (...) {
            prom.set_exception(std::current_exception());
        }
    });
    
    return fut;
}
```

### Memory Ordering and Futures

Future operations provide acquire-release semantics:

```cpp
// set_value() provides release semantics
void set_value(const T& value) {
    std::atomic_thread_fence(std::memory_order_release);
    // Set value and notify
}

// get() provides acquire semantics
T get() {
    wait();
    std::atomic_thread_fence(std::memory_order_acquire);
    return value_;
}
```

### Exception Handling

```cpp
// Exception propagation through futures
void worker_with_exception(std::promise<int> p) {
    try {
        throw std::runtime_error("Error in worker");
    } catch (...) {
        // Capture current exception
        p.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(worker_with_exception, std::move(prom));
    
    try {
        int result = fut.get();  // Re-throws the exception
    } catch (const std::runtime_error& e) {
        std::cout << "Caught exception: " << e.what() << "\n";
    }
    
    t.join();
    return 0;
}
```

### Reference Implementation for void

```cpp
// Specialization for void futures
template<>
class future<void> {
public:
    void get() {
        if (!state_) {
            throw std::future_error(std::future_errc::no_state);
        }
        state_->get();  // Just waits for completion
    }
    
    // ... other methods similar to non-void version
};

template<>
class promise<void> {
public:
    void set_value() {
        state_->set_value();  // Signal completion without value
    }
    
    // ... other methods
};
```

## Best Practices

1. Always check future::valid() before using a future
2. Handle exceptions from futures with try-catch
3. Use std::launch::async for true parallelism
4. Use std::launch::deferred for lazy evaluation
5. Prefer std::async over manual thread/future management
6. Use shared_future when multiple threads need the same result
7. Be aware of blocking behavior in get() and wait()
8. Use wait_for/wait_until for timeout-based operations
9. Don't forget to join threads that own promises
10. Use packaged_task when you need to store tasks for later execution
