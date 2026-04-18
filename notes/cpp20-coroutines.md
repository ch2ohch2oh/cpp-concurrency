# C++20 Coroutines: Usage and Implementation

## Practical Usage

### Basic Coroutine with co_await

```cpp
#include <coroutine>
#include <iostream>

struct Awaiter {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() { std::cout << "Resumed\n"; }
};

struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

Task my_coroutine() {
    std::cout << "Started\n";
    co_await Awaiter{};
    std::cout << "After await\n";
}

int main() {
    my_coroutine();
    return 0;
}
```

### Generator with co_yield

```cpp
#include <coroutine>
#include <iostream>

template<typename T>
struct Generator {
    struct promise_type {
        T current_value;
        
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_void() {}
        
        std::suspend_always yield_value(T value) {
            current_value = value;
            return {};
        }
        
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
```

### Async Task with co_return

```cpp
#include <coroutine>
#include <future>
#include <thread>

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
```

### Async/Await Pattern

```cpp
#include <coroutine>
#include <chrono>
#include <thread>
#include <iostream>

struct AsyncAwaiter {
    std::chrono::milliseconds duration;
    
    bool await_ready() { return false; }
    
    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([handle, this]() {
            std::this_thread::sleep_for(duration);
            handle.resume();
        }).detach();
    }
    
    void await_resume() {}
};

struct AsyncTask {
    struct promise_type {
        AsyncTask get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
};

AsyncTask async_delay() {
    std::cout << "Before delay\n";
    co_await AsyncAwaiter{std::chrono::milliseconds(100)};
    std::cout << "After delay\n";
}

int main() {
    async_delay();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}
```

### Realistic Example: Async HTTP Client

```cpp
#include <coroutine>
#include <future>
#include <string>

struct HttpResponse {
    int status_code;
    std::string body;
};

struct HttpAwaiter {
    std::string url;
    std::future<HttpResponse> future;
    
    HttpAwaiter(std::string u) : url(std::move(u)) {}
    
    bool await_ready() {
        // Start async HTTP request
        future = std::async(std::launch::async, [this]() {
            // Simulate HTTP request
            return HttpResponse{200, "Response body"};
        });
        return false;
    }
    
    void await_suspend(std::coroutine_handle<>) {}
    
    HttpResponse await_resume() {
        return future.get();
    }
};

struct AsyncTask {
    struct promise_type {
        std::promise<HttpResponse> prom;
        
        AsyncTask get_return_object() {
            return AsyncTask{prom.get_future()};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(HttpResponse value) {
            prom.set_value(value);
        }
        
        void unhandled_exception() {
            prom.set_exception(std::current_exception());
        }
    };
    
    std::future<HttpResponse> future;
    
    AsyncTask(std::future<HttpResponse> f) : future(std::move(f)) {}
    
    HttpResponse get() {
        return future.get();
    }
};

AsyncTask fetch_data() {
    HttpResponse response = co_await HttpAwaiter{"https://example.com"};
    co_return response;
}
```

## Underlying Implementation

### Coroutine State Machine

```cpp
// Conceptual coroutine state machine
enum class CoroutineState {
    Created,
    Running,
    Suspended,
    Completed
};

struct CoroutineFrame {
    CoroutineState state;
    void* promise;
    void* resume_point;
    // Local variables
};
```

### Coroutine Handle

```cpp
// std::coroutine_handle implementation
namespace std {
    template<typename Promise = void>
    struct coroutine_handle {
        void* frame_;  // Pointer to coroutine frame
        
        void resume() {
            // Jump to coroutine resume point
            __builtin_coro_resume(frame_);
        }
        
        void destroy() {
            // Destroy coroutine frame
            __builtin_coro_destroy(frame_);
        }
        
        bool done() const {
            return __builtin_coro_done(frame_);
        }
        
        Promise& promise() const {
            return *static_cast<Promise*>(__builtin_coro_promise(frame_));
        }
    };
}
```

### Promise Type Requirements

```cpp
struct promise_type {
    // Required: create return object
    MyTask get_return_object();
    
    // Required: initial suspend behavior
    std::suspend_never initial_suspend();
    // or: std::suspend_always initial_suspend();
    
    // Required: final suspend behavior
    std::suspend_never final_suspend() noexcept;
    // or: std::suspend_always final_suspend() noexcept;
    
    // Required for co_return value
    void return_value(T value);
    // or for co_return void: void return_void();
    
    // Required: exception handling
    void unhandled_exception();
    
    // Optional: co_yield
    std::suspend_always yield_value(T value);
    
    // Optional: custom allocation
    static void* operator new(size_t size);
    static void operator delete(void* ptr);
};
```

### Awaiter Requirements

```cpp
struct Awaiter {
    // Required: check if ready to proceed
    bool await_ready();
    
    // Required: suspend or resume immediately
    void await_suspend(std::coroutine_handle<> handle);
    // or: bool await_suspend(std::coroutine_handle<> handle);
    
    // Required: value to return from co_await
    T await_resume();
};
```

### Standard Awaitables

```cpp
// std::suspend_always - always suspend
struct suspend_always {
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};

// std::suspend_never - never suspend
struct suspend_never {
    bool await_ready() noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};
```

### Coroutine Frame Layout

```cpp
// Compiler-generated coroutine frame layout
struct CoroutineFrame {
    // Header
    void* vtable;
    size_t resume_point;
    
    // Promise object
    Promise promise;
    
    // Local variables
    LocalVar1 var1;
    LocalVar2 var2;
    
    // Exception object (if any)
    std::exception_ptr exception;
    
    // Destructor info
    // ...
};
```

### Compiler Transformation

```cpp
// Source code
Task my_coroutine(int x) {
    int y = x * 2;
    co_await Awaiter{};
    co_return y + 1;
}

// Compiler transforms to:
struct my_coroutine_frame {
    int x;
    int y;
    int resume_index;
    // ...
};

void my_coroutine_body(my_coroutine_frame* frame) {
    try {
        switch (frame->resume_index) {
            case 0:
                frame->y = frame->x * 2;
                frame->resume_index = 1;
                co_await Awaiter{};
                // fall through
            case 1:
                co_return frame->y + 1;
        }
    } catch (...) {
        frame->promise.unhandled_exception();
    }
}
```

### Custom Allocator

```cpp
struct promise_type {
    static void* operator new(size_t size) {
        std::cout << "Allocating coroutine: " << size << " bytes\n";
        return ::operator new(size);
    }
    
    static void operator delete(void* ptr) {
        std::cout << "Deallocating coroutine\n";
        ::operator delete(ptr);
    }
    
    // ... other promise methods
};
```

### Symmetric Transfer

```cpp
// Efficient coroutine switching
struct SymmetricTransferAwaiter {
    std::coroutine_handle<> to_resume;
    
    bool await_ready() noexcept { return false; }
    
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> from) noexcept {
        // Direct transfer without going through scheduler
        return to_resume;
    }
    
    void await_resume() noexcept {}
};
```

### Coroutine Exception Handling

```cpp
struct promise_type {
    std::exception_ptr exception;
    
    void unhandled_exception() {
        exception = std::current_exception();
    }
    
    // Check for exception in await_resume
    T await_resume() {
        if (exception) {
            std::rethrow_exception(exception);
        }
        return value;
    }
};
```

### Coroutine Lifecycle

```cpp
// Lifecycle states
// 1. Created: coroutine created but not started
// 2. Running: coroutine is executing
// 3. Suspended: coroutine paused at co_await
// 4. Completed: coroutine finished or threw exception

Task task = my_coroutine();  // Created
task.handle.resume();        // Running -> Suspended
task.handle.resume();        // Suspended -> Completed
task.handle.destroy();       // Cleanup
```

### Coroutine Stackless Nature

```cpp
// Coroutines are stackless - no separate stack
// They use heap-allocated frame for state

// This enables:
// - Low memory overhead
// - Fast context switching
// - Millions of coroutines possible

// Unlike threads:
// - No separate stack per coroutine
// - No preemption
// - Explicit yield points
```

## Best Practices

1. Use RAII for coroutine handle management
2. Implement proper exception handling in promise type
3. Consider custom allocators for performance-critical code
4. Use symmetric transfer for efficient coroutine switching
5. Be aware of coroutine frame size
6. Use co_return instead of return in coroutines
7. Implement await_ready optimization when possible
8. Use co_await for async operations
9. Use co_yield for generators
10. Test coroutine destruction and exception paths
