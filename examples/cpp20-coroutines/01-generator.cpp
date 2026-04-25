#include <coroutine>
#include <iostream>

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;

        // Create the return object (Generator) from the promise
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // Suspend immediately after creation (lazy generator)
        std::suspend_always initial_suspend() { return {}; }

        // Suspend at completion (keeps frame alive for cleanup)
        std::suspend_always final_suspend() noexcept { return {}; }

        // Handle co_return without a value
        void return_void() {}

        // Handle co_yield - store value and suspend
        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }

        // Handle unhandled exceptions
        void unhandled_exception() { std::terminate(); }
    };
    
    std::coroutine_handle<promise_type> handle;
    
    Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    ~Generator() {
        if (handle) handle.destroy();
    }
    
    bool next() {
        handle.resume();
        return !handle.done();
    }
    
    T value() const {
        return handle.promise().current_value;
    }
};

Generator<int> range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

int main() {
    auto gen = range(0, 5);
    while (gen.next()) {
        std::cout << gen.value() << " ";
    }
    std::cout << "\n";
    return 0;
}
