// Coroutine Handle Examples
// This file demonstrates practical usage of std::coroutine_handle

#include <iostream>
#include <coroutine>
#include <exception>

// Example 1: Basic Generator with coroutine handle
struct Generator {
    struct promise_type {
        int current_value;

        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(int value) {
            current_value = value;
            return {};
        }

        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    Generator(std::coroutine_handle<promise_type> h) : handle(h) {}

    ~Generator() {
        if (handle) handle.destroy();
    }

    // Prevent copying
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    // Allow moving
    Generator(Generator&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    int next() {
        handle.resume();
        return handle.promise().current_value;
    }

    bool done() const {
        return handle.done();
    }
};

Generator range(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

// Example 2: RAII wrapper for coroutine handle management
class Task {
    std::coroutine_handle<> handle;

public:
    Task(std::coroutine_handle<> h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }

    // Move-only type
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    void resume() {
        if (handle && !handle.done()) {
            handle.resume();
        }
    }

    bool done() const {
        return handle.done();
    }
};

// Example 3: Type-erased vs typed handles
struct TypedPromise {
    int value = 42;
};

void demonstrate_handle_types() {
    // Type-erased handle - can refer to any coroutine
    std::coroutine_handle<> generic_handle;

    // Typed handle - provides access to promise
    std::coroutine_handle<TypedPromise> typed_handle;

    // Conversion from typed to type-erased
    std::coroutine_handle<> erased = typed_handle;

    std::cout << "Generic handle: " << (generic_handle ? "valid" : "null") << "\n";
    std::cout << "Typed handle: " << (typed_handle ? "valid" : "null") << "\n";
}

// Example 4: Null handle safety
void safe_resume(std::coroutine_handle<> handle) {
    if (handle) {  // Check for null
        if (!handle.done()) {
            handle.resume();
        }
    }

    // Or use address() for explicit null check
    if (handle.address() != nullptr) {
        // Safe to use
    }
}

int main() {
    std::cout << "=== Example 1: Generator with coroutine handle ===\n";
    auto gen = range(1, 5);
    while (!gen.done()) {
        std::cout << gen.next() << " ";
    }
    std::cout << "\n\n";

    std::cout << "=== Example 2: Handle types ===\n";
    demonstrate_handle_types();
    std::cout << "\n";

    std::cout << "=== Example 3: Null handle safety ===\n";
    std::coroutine_handle<> null_handle;
    safe_resume(null_handle);
    std::cout << "Null handle handled safely\n";

    return 0;
}
