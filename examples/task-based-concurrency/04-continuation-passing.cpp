#include <future>
#include <iostream>
#include <algorithm>
#include <thread>
#include <type_traits>

template<typename T>
class Task {
private:
    std::shared_ptr<std::promise<T>> promise_;
    std::shared_ptr<std::thread> thread_;
    
public:
    Task() : promise_(std::make_shared<std::promise<T>>()) {}
    
    template<typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, Task<T>>>>
    explicit Task(F&& func) : promise_(std::make_shared<std::promise<T>>()) {
        thread_ = std::make_shared<std::thread>([this, func = std::forward<F>(func)]() mutable {
            try {
                promise_->set_value(func());
            } catch (...) {
                promise_->set_exception(std::current_exception());
            }
        });
    }
    
    ~Task() {
        if (thread_ && thread_->joinable()) {
            thread_->join();
        }
    }
    
    std::future<T> get_future() {
        return promise_->get_future();
    }
    
    template<typename F>
    auto then(F&& func) -> Task<std::invoke_result_t<F, T>> {
        using ResultType = std::invoke_result_t<F, T>;
        
        Task<ResultType> next_task;
        
        // Launch a thread to wait for this task and then execute the continuation
        next_task.thread_ = std::make_shared<std::thread>([this, func = std::forward<F>(func), next_task]() mutable {
            try {
                T result = promise_->get_future().get();
                next_task.promise_->set_value(func(result));
            } catch (...) {
                next_task.promise_->set_exception(std::current_exception());
            }
        });
        
        return next_task;
    }
};

// Helper function to deduce task type from lambda
template<typename F>
auto make_task(F&& func) -> Task<std::invoke_result_t<F>> {
    return Task<std::invoke_result_t<F>>(std::forward<F>(func));
}

/*
 * Comparison with Facebook Folly's Future.then():
 *
 * folly::Future<int> result = folly::makeFuture(5)
 *     .then([](int x) { return x * x; })
 *     .then([](int x) { return x + 10; })
 *     .then([](int x) { return x * 2; });
 *
 * Key differences from our implementation:
 * - Folly uses a thread pool instead of spawning a thread per task
 * - Better exception handling with .thenValue() and .thenTry()
 * - Supports executors to control where tasks run
 * - Has .via() to specify execution context
 * - Cancellation support via .cancel()
 * - More sophisticated type deduction
 */

int main() {
    // Fluent chainable API: make_task(func).then(...).then(...)
    std::cout << "Starting task chain with value 5\n";
    
    auto result = make_task([]() { return 5; })
        .then([](int x) { 
            std::cout << "Step 1: Squaring " << x << "\n";
            return x * x; 
        })
        .then([](int x) { 
            std::cout << "Step 2: Adding 10 to " << x << "\n";
            return x + 10; 
        })
        .then([](int x) { 
            std::cout << "Step 3: Multiplying " << x << " by 2\n";
            return x * 2; 
        })
        .get_future()
        .get();
    
    std::cout << "Final result: " << result << "\n";
    
    // Demonstrate with a different type (string)
    auto str_result = make_task([]() { return std::string("hello"); })
        .then([](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        })
        .then([](std::string s) {
            return s + " WORLD";
        })
        .get_future()
        .get();
    
    std::cout << "String result: " << str_result << "\n";
    
    return 0;
}
