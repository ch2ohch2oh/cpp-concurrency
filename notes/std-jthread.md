# std::jthread: Usage and Implementation

## Practical Usage

### Basic jthread Usage

```cpp
#include <thread>
#include <stop_token>
#include <iostream>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::cout << "Working...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Stopping...\n";
}

int main() {
    std::jthread t(worker);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    t.request_stop();  // Request thread to stop
    t.join();  // Automatic join on destruction
    
    return 0;
}
```

### jthread with Automatic Join

```cpp
#include <thread>
#include <iostream>

void task(int id) {
    std::cout << "Task " << id << " running\n";
}

int main() {
    {
        std::jthread t1(task, 1);
        std::jthread t2(task, 2);
        // Automatic join when leaving scope
    }
    std::cout << "Threads joined\n";
    
    return 0;
}
```

### Stop Callback

```cpp
#include <thread>
#include <stop_token>
#include <iostream>

void worker_with_callback(std::stop_token stoken) {
    std::stop_callback callback(stoken, []() {
        std::cout << "Stop requested, cleaning up...\n";
    });
    
    while (!stoken.stop_requested()) {
        // Do work
    }
}

int main() {
    std::jthread t(worker_with_callback);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    t.request_stop();
    
    return 0;
}
```

### Interruptible Wait

```cpp
#include <thread>
#include <stop_token>
#include <condition_variable>
#include <mutex>
#include <iostream>

std::mutex mtx;
std::condition_variable_any cv;
bool ready = false;

void waiter(std::stop_token stoken) {
    std::unique_lock<std::mutex> lock(mtx);
    
    // Wait with stop token support
    cv.wait(lock, stoken, [] { return ready; });
    
    if (stoken.stop_requested()) {
        std::cout << "Wait interrupted\n";
    } else {
        std::cout << "Condition met\n";
    }
}

int main() {
    std::jthread t(waiter);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    t.request_stop();  // Interrupt wait
    
    return 0;
}
```

### Realistic Example: Cancellable Task

```cpp
#include <thread>
#include <stop_token>
#include <atomic>
#include <vector>
#include <iostream>

class CancellableProcessor {
private:
    std::jthread worker_;
    std::atomic<bool> processing_;
    
public:
    CancellableProcessor() : processing_(false) {}
    
    void start_processing(const std::vector<int>& data) {
        processing_ = true;
        worker_ = std::jthread([this, data](std::stop_token stoken) {
            for (int item : data) {
                if (stoken.stop_requested()) {
                    std::cout << "Processing cancelled\n";
                    break;
                }
                
                // Process item
                std::cout << "Processing: " << item << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            processing_ = false;
        });
    }
    
    void cancel() {
        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
    }
    
    bool is_processing() const {
        return processing_;
    }
};

int main() {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 0);
    
    CancellableProcessor processor;
    processor.start_processing(data);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    processor.cancel();
    
    return 0;
}
```

## Underlying Implementation

### std::jthread Implementation

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

1. Use jthread instead of thread for automatic resource management
2. Pass stop_token as first argument to worker functions
3. Use stop_callback for cleanup on cancellation
4. Check stop_requested() frequently in long-running loops
5. Use condition_variable_any with stop_token for interruptible waits
6. Design worker functions to be cooperative (check for stop)
7. Avoid blocking operations that can't be interrupted
8. Use request_stop() before destructor for explicit cancellation
9. Be aware of callback execution context (synchronous)
10. Test cancellation paths thoroughly
