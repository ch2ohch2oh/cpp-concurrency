#include <coroutine>
#include <future>
#include <thread>
#include <chrono>
#include <iostream>
#include <tuple>

// Async task that can be launched and awaited separately
struct AsyncTask {
    struct promise_type {
        std::promise<std::string> prom;

        AsyncTask get_return_object() {
            return AsyncTask{prom.get_future()};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(std::string value) {
            prom.set_value(value);
        }

        void unhandled_exception() {
            prom.set_exception(std::current_exception());
        }
    };

    std::future<std::string> future;

    AsyncTask(std::future<std::string> f) : future(std::move(f)) {}

    std::string get() {
        return future.get();
    }
};

// Launch async I/O operation
AsyncTask async_io(std::string data, std::chrono::milliseconds delay) {
    std::this_thread::sleep_for(delay);  // Simulate I/O delay
    co_return data;
}

// Awaiter for multiple tasks (when_all)
struct WhenAllAwaiter {
    std::future<std::string> task1;
    std::future<std::string> task2;

    WhenAllAwaiter(AsyncTask t1, AsyncTask t2)
        : task1(std::move(t1.future)), task2(std::move(t2.future)) {}

    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        // Launch thread to wait for both tasks
        std::thread([this, handle]() {
            task1.wait();  // Wait for both to complete
            task2.wait();
            handle.resume();  // Resume coroutine
        }).detach();
    }

    std::tuple<std::string, std::string> await_resume() {
        return {task1.get(), task2.get()};
    }
};

// Coroutine that launches concurrent I/O operations
AsyncTask async_compute(int x) {
    // Launch both I/O operations concurrently
    auto config_task = async_io("config_value", std::chrono::milliseconds(1000));
    auto data_task = async_io("network_data", std::chrono::milliseconds(1000));

    // co_await both tasks to complete
    auto [config, data] = co_await WhenAllAwaiter{std::move(config_task), std::move(data_task)};
    std::cout << "Read config: " << config << "\n";
    std::cout << "Read data: " << data << "\n";

    // Return the combined data
    co_return config + " + " + data;
}

int main() {
    auto task = async_compute(42);
    std::cout << "Result: " << task.get() << "\n";
    return 0;
}
