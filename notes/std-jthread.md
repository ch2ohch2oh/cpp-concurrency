# std::jthread (C++20)

## Motivation

`std::jthread` is a C++20 improvement over `std::thread` that addresses two major pain points: automatic joining and cooperative cancellation. Unlike `std::thread`, which requires explicit joining and has no built-in cancellation mechanism, `std::jthread` automatically joins on destruction and provides a standard way to request thread termination.

The key motivation is to make thread management safer and more ergonomic:
- **Automatic joining**: No risk of calling `std::terminate` if you forget to join
- **Cooperative cancellation**: Built-in stop token mechanism for graceful shutdown
- **Exception safety**: RAII semantics ensure proper cleanup
- **Better integration**: Works seamlessly with condition variables and other synchronization primitives

`std::jthread` is particularly useful for:
- Long-running background tasks
- Worker threads that need graceful shutdown
- Applications with many threads where manual join management is error-prone
- Scenarios requiring cooperative cancellation

## Practical Usage

See the `examples/std-jthread/` directory for complete working examples:
- `01-basic-jthread.cpp` - Basic jthread with automatic joining
- `02-stop-token.cpp` - Cancellable worker with stop token

### Key Features

**Automatic Join**: `std::jthread` automatically joins in its destructor, preventing `std::terminate` calls from forgotten joins.

**Stop Token**: Each `jthread` has an associated `stop_token` that can be used to cooperatively request thread termination.

**Stop Callback**: Register callbacks to execute when a stop is requested, useful for cleanup operations.

**Interruptible Waits**: Condition variables support stop tokens, allowing waits to be interrupted by stop requests.

### Common Patterns

**Basic Usage**: Pass a function to `std::jthread` constructor. The thread automatically joins when the `jthread` object is destroyed.

**Stop Token**: Pass `std::stop_token` as the first argument to worker functions. Check `stop_requested()` periodically to support cooperative cancellation.

**Stop Callback**: Use `std::stop_callback` to register cleanup actions that execute when a stop is requested.

**Interruptible Waits**: Use `condition_variable_any` with stop tokens to make condition variable waits interruptible.

## Pros

- **Automatic cleanup**: RAII semantics ensure threads are always joined
- **Cooperative cancellation**: Standard mechanism for requesting thread termination
- **Exception safe**: No risk of `std::terminate` from forgotten joins
- **Better API**: More ergonomic than manual `std::thread` management
- **Standard integration**: Works with condition variables and other primitives
- **Backward compatible**: Can be used alongside `std::thread`

## Cons

- **C++20 only**: Requires a C++20-compatible compiler
- **Cooperative only**: Threads must check stop token; cannot force termination
- **Overhead**: Stop token mechanism adds minimal overhead
- **Limited control**: Less fine-grained control than manual thread management
- **Migration effort**: Existing code using `std::thread` needs refactoring

## Underlying Implementation

### std::jthread Implementation

`std::jthread` is essentially a wrapper around `std::thread` with a `stop_source` for cancellation and automatic joining in the destructor:

```cpp
#include <thread>
#include <stop_token>

namespace std {
    class jthread {
    public:
        // Constructors
        jthread() noexcept = default;
        
        template<class Function, class... Args>
        explicit jthread(Function&& f, Args&&... args) {
            auto wrapper = [f = std::forward<Function>(f), 
                           args = std::make_tuple(std::forward<Args>(args)...),
                           &stoken = source_]() mutable {
                std::apply([&](auto&&... a) {
                    std::invoke(f, stoken, std::forward<decltype(a)>(a)...);
                }, args);
            };
            
            thread_ = std::thread(std::move(wrapper));
        }
        
        // Destructor - automatic join
        ~jthread() {
            if (joinable()) {
                request_stop();
                join();
            }
        }
        
        // Delete copy operations
        jthread(const jthread&) = delete;
        jthread& operator=(const jthread&) = delete;
        
        // Move operations
        jthread(jthread&&) noexcept = default;
        jthread& operator=(jthread&&) noexcept = default;
        
        // Stop token access
        std::stop_source get_stop_source() noexcept {
            return source_;
        }
        
        std::stop_token get_stop_token() const noexcept {
            return source_.get_token();
        }
        
        bool request_stop() noexcept {
            return source_.request_stop();
        }
        
        // Thread operations
        bool joinable() const noexcept {
            return thread_.joinable();
        }
        
        void join() {
            thread_.join();
        }
        
        void detach() {
            thread_.detach();
        }
        
        std::thread::id get_id() const noexcept {
            return thread_.get_id();
        }
        
        std::thread::native_handle_type native_handle() {
            return thread_.native_handle();
        }
        
    private:
        std::stop_source source_;
        std::thread thread_;
    };
}
```

### std::stop_token Implementation

The stop token is the read-only interface for checking stop requests:

```cpp
#include <thread>
#include <stop_token>

namespace std {
    class jthread {
    public:
        // Constructors
        jthread() noexcept = default;
        
        template<class Function, class... Args>
        explicit jthread(Function&& f, Args&&... args) {
            auto wrapper = [f = std::forward<Function>(f), 
                           args = std::make_tuple(std::forward<Args>(args)...),
                           &stoken = source_]() mutable {
                std::apply([&](auto&&... a) {
                    std::invoke(f, stoken, std::forward<decltype(a)>(a)...);
                }, args);
            };
            
            thread_ = std::thread(std::move(wrapper));
        }
        
        // Destructor - automatic join
        ~jthread() {
            if (joinable()) {
                request_stop();
                join();
            }
        }
        
        // Delete copy operations
        jthread(const jthread&) = delete;
        jthread& operator=(const jthread&) = delete;
        
        // Move operations
        jthread(jthread&&) noexcept = default;
        jthread& operator=(jthread&&) noexcept = default;
        
        // Stop token access
        std::stop_source get_stop_source() noexcept {
            return source_;
        }
        
        std::stop_token get_stop_token() const noexcept {
            return source_.get_token();
        }
        
        bool request_stop() noexcept {
            return source_.request_stop();
        }
        
        // Thread operations
        bool joinable() const noexcept {
            return thread_.joinable();
        }
        
        void join() {
            thread_.join();
        }
        
        void detach() {
            thread_.detach();
        }
        
        std::thread::id get_id() const noexcept {
            return thread_.get_id();
        }
        
        std::thread::native_handle_type native_handle() {
            return thread_.native_handle();
        }
        
    private:
        std::stop_source source_;
        std::thread thread_;
    };
}
```

### std::stop_token Implementation

```cpp
namespace std {
    class stop_token {
    public:
        stop_token() noexcept = default;
        
        stop_token(const stop_token&) noexcept = default;
        stop_token& operator=(const stop_token&) noexcept = default;
        
        ~stop_token() = default;
        
        bool stop_requested() const noexcept {
            if (!state_) return false;
            return state_->stop_requested_.load(std::memory_order_acquire);
        }
        
        bool stop_possible() const noexcept {
            return state_ != nullptr;
        }
        
    private:
        friend class stop_source;
        friend class stop_callback;
        
        struct stop_state {
            std::atomic<bool> stop_requested_{false};
            std::mutex mtx_;
            std::vector<std::function<void()>> callbacks_;
        };
        
        std::shared_ptr<stop_state> state_;
    };
}
```

### std::stop_source Implementation

```cpp
namespace std {
    class stop_source {
    public:
        stop_source() : state_(std::make_shared<stop_token::stop_state>()) {}
        
        explicit stop_source(std::nostopstate_t) noexcept : state_(nullptr) {}
        
        stop_source(const stop_source&) noexcept = default;
        stop_source& operator=(const stop_source&) noexcept = default;
        
        stop_source(stop_source&&) noexcept = default;
        stop_source& operator=(stop_source&&) noexcept = default;
        
        ~stop_source() = default;
        
        std::stop_token get_token() const noexcept {
            return stop_token{state_};
        }
        
        bool request_stop() noexcept {
            if (!state_) return false;
            
            std::lock_guard<std::mutex> lock(state_->mtx_);
            
            if (state_->stop_requested_.load(std::memory_order_acquire)) {
                return false;  // Already stopped
            }
            
            state_->stop_requested_.store(true, std::memory_order_release);
            
            // Execute callbacks
            for (auto& callback : state_->callbacks_) {
                callback();
            }
            state_->callbacks_.clear();
            
            return true;
        }
        
        bool stop_possible() const noexcept {
            return state_ != nullptr;
        }
        
    private:
        std::shared_ptr<stop_token::stop_state> state_;
    };
    
    inline constexpr std::nostopstate_t nostopstate{};
}
```

### std::stop_callback Implementation

```cpp
namespace std {
    template<typename Callback>
    class stop_callback {
    public:
        using callback_type = Callback;
        
        template<class C>
        explicit stop_callback(const stop_token& stoken, C&& cb) 
            : callback_(std::forward<C>(cb)) {
            
            if (stoken.state_) {
                std::lock_guard<std::mutex> lock(stoken.state_->mtx_);
                
                if (stoken.state_->stop_requested_.load(std::memory_order_acquire)) {
                    // Already stopped, execute immediately
                    callback_();
                } else {
                    // Register callback
                    stoken.state_->callbacks_.push_back([this]() {
                        this->callback_();
                    });
                    registered_ = true;
                }
            }
        }
        
        ~stop_callback() {
            if (registered_ && state_) {
                std::lock_guard<std::mutex> lock(state_->mtx_);
                // Remove callback from list
                // (Simplified - actual implementation more complex)
            }
        }
        
        stop_callback(const stop_callback&) = delete;
        stop_callback& operator=(const stop_callback&) = delete;
        
    private:
        Callback callback_;
        std::shared_ptr<stop_token::stop_state> state_;
        bool registered_ = false;
    };
    
    template<typename Callback>
    stop_callback(stop_token, Callback) -> stop_callback<Callback>;
}
```

### Stop Token with Condition Variable

```cpp
namespace std {
    template<class Lock>
    void condition_variable_any::wait(Lock& lock, stop_token stoken) {
        while (!pred()) {
            if (stoken.stop_requested()) {
                return;  // Stop requested
            }
            cv_.wait_for(lock, std::chrono::milliseconds(10));
        }
    }
    
    template<class Lock, class Predicate>
    bool condition_variable_any::wait(Lock& lock, stop_token stoken, Predicate pred) {
        while (!pred()) {
            if (stoken.stop_requested()) {
                return pred();  // Return final state
            }
            cv_.wait_for(lock, std::chrono::milliseconds(10));
        }
        return true;
    }
}
```

### Stop Token Polling Pattern

```cpp
void polling_worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // Do a unit of work
        process_item();
        
        // Check for stop request frequently
        if (stoken.stop_requested()) {
            cleanup();
            break;
        }
    }
}
```

### Stop Token with Interruptible Operations

```cpp
template<typename T>
class InterruptibleQueue {
private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable_any cv_;
    
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(item);
        }
        cv_.notify_one();
    }
    
    bool try_pop(T& item, std::stop_token stoken) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        return cv_.wait(lock, stoken, [this] {
            return !queue_.empty();
        }) && (item = queue_.front(), queue_.pop(), true);
    }
};
```

### Stop Token Thread Safety

```cpp
// Stop token operations are thread-safe
// - stop_requested() can be called from any thread
// - request_stop() is thread-safe
// - Callbacks are executed under lock

// Memory ordering ensures visibility
bool stop_requested() const noexcept {
    return state_->stop_requested_.load(std::memory_order_acquire);
}

bool request_stop() noexcept {
    // Acquire lock
    std::lock_guard<std::mutex> lock(state_->mtx_);
    
    // Release semantics for visibility
    state_->stop_requested_.store(true, std::memory_order_release);
    
    // Execute callbacks
    // ...
}
```

### Comparison with std::thread

```cpp
// std::thread
std::thread t(worker);
t.join();  // Must explicitly join
// If not joined, std::terminate called in destructor

// std::jthread
std::jthread jt(worker);
// Automatic join in destructor
// Built-in cancellation support
```

## Best Practices

1. **Use jthread instead of thread**: For automatic resource management and exception safety
2. **Pass stop_token as first argument**: Follow the convention for worker functions
3. **Use stop_callback for cleanup**: Register cleanup actions when stop is requested
4. **Check stop_requested() frequently**: In long-running loops to support responsive cancellation
5. **Use condition_variable_any**: With stop_token for interruptible waits
6. **Design cooperative workers**: Ensure threads can respond to stop requests
7. **Avoid uninterruptible blocking**: Use interruptible operations or timeouts
8. **Request stop explicitly**: Before destructor for predictable shutdown timing
9. **Be aware of callback context**: Callbacks execute synchronously in request_stop()
10. **Test cancellation paths**: Thoroughly test all cancellation scenarios
