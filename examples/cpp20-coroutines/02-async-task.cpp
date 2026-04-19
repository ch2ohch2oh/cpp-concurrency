#include <coroutine>
#include <future>
#include <thread>
#include <iostream>

struct AsyncTask {
    struct promise_type {
        std::promise<int> prom;
        
        AsyncTask get_return_object() {
            return AsyncTask{prom.get_future()};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(int value) {
            prom.set_value(value);
        }
        
        void unhandled_exception() {
            prom.set_exception(std::current_exception());
        }
    };
    
    std::future<int> future;
    
    AsyncTask(std::future<int> f) : future(std::move(f)) {}
    
    int get() {
        return future.get();
    }
};

AsyncTask async_compute(int x) {
    co_return x * x;
}

int main() {
    auto task = async_compute(42);
    std::cout << "Result: " << task.get() << "\n";
    return 0;
}
