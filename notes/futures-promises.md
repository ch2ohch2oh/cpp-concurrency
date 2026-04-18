# Futures and Promises: Usage and Implementation

## Motivation: Why Futures and Promises?

### The Problem: Thread Communication Without Futures

Before futures and promises, communicating between threads required manual synchronization with shared state, mutexes, and condition variables. This approach had several problems:

**1. Verbose and error-prone:**
```cpp
// Manual thread communication (the old way)
int result;
bool ready = false;
std::mutex mtx;
std::condition_variable cv;

void worker() {
    std::lock_guard<std::mutex> lock(mtx);
    result = 42;
    ready = true;
    cv.notify_one();
}

int main() {
    std::thread t(worker);
    
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    std::cout << result << "\n";
    
    t.join();
}
```

**2. Exception handling is difficult:** If the worker throws an exception, how do you propagate it to the waiting thread? You need additional shared state and error handling logic.

**3. No type safety for the communication channel:** The shared state is just a variable; nothing enforces that it's used correctly as a one-time channel.

**4. Hard to compose:** Waiting for multiple threads to complete requires complex coordination logic.

### The Solution: Futures and Promises

Futures and promises provide a **one-time, type-safe channel** for thread communication:

- **Promise**: The producer side - sets the value (or exception) exactly once
- **Future**: The consumer side - retrieves the value (or exception), blocking if not ready

**Key benefits:**

1. **Clean API**: No manual mutex/condition variable boilerplate
2. **Exception propagation**: Exceptions in the producer thread automatically propagate to the consumer
3. **Type safety**: The channel type is enforced at compile time
4. **One-time semantics**: Prevents multiple reads/writes bugs
5. **Composable**: Easy to wait for multiple futures, chain operations, etc.
6. **Separation of concerns**: Producer doesn't need to know about the consumer

### When to Use Futures/Promises

- **Async task execution**: Launch a task and get its result later
- **Parallel computation**: Break work into independent parallel tasks
- **Thread pool results**: Return results from worker threads
- **API boundaries**: Cleanly separate async work from synchronous code

### When NOT to Use Futures/Promises

- **Continuous data streams**: Use queues/channels instead
- **Multiple consumers**: Use `shared_future` or other synchronization
- **Simple callbacks**: If you just need notification, consider condition variables

## Practical Usage

### std::future and std::promise - Basic Usage

See `examples/futures-promises/01-basic-promise-future.cpp`

### std::async - Asynchronous Task Execution

See `examples/futures-promises/02-async-task.cpp`

### std::packaged_task - Task with Future

See `examples/futures-promises/03-packaged-task.cpp`

### Multiple Futures with std::when_all (C++23) or Manual Implementation

See `examples/futures-promises/04-multiple-futures.cpp`

### std::shared_future - Multiple Readers

See `examples/futures-promises/05-shared-future.cpp`

### Timeout with future::wait_for

See `examples/futures-promises/06-timeout.cpp`

### Realistic Example: Parallel MapReduce

See `examples/futures-promises/07-parallel-mapreduce.cpp`

## Underlying Implementation

### Future/Promise Architecture

The future/promise pattern implements a one-time channel:

```
Producer Thread                      Consumer Thread
       |                                    |
       v                                    v
  [Promise] -----> [Shared State] <----- [Future]
       |                                    |
       v                                    v
Set value/exception                  Wait for result
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
