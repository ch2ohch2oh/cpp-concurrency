# C++20 Coroutines

## Motivation

C++20 coroutines provide a powerful way to write asynchronous code that looks synchronous. Unlike traditional callback-based async code, coroutines allow you to write linear, readable code while still benefiting from non-blocking execution.

The key motivation is to simplify asynchronous programming:
- **Readability**: Async code reads like synchronous code
- **Efficiency**: Stackless coroutines have low memory overhead
- **Composability**: Easy to chain and combine async operations
- **Performance**: No context switching overhead compared to threads
- **Scalability**: Can handle millions of concurrent operations

Coroutines are particularly useful for:
- Asynchronous I/O (network, file operations)
- Generators and lazy sequences
- Cooperative multitasking
- Event-driven applications
- Game development (async asset loading, AI behaviors)

## Practical Usage

See the `examples/cpp20-coroutines/` directory for complete working examples:
- `01-generator.cpp` - Generator pattern with co_yield
- `02-async-task.cpp` - Async tasks with co_return

### Key Concepts

**co_await**: Suspend the coroutine until an asynchronous operation completes. The awaitable object controls suspension and resumption behavior.

**co_yield**: Produce a value and suspend the coroutine. Used in generators to produce sequences of values lazily.

**co_return**: Return a value from a coroutine. Similar to regular return but for coroutines.

**Promise Type**: Customizable object that controls coroutine behavior (allocation, suspension, return values, exception handling).

**Coroutine Handle**: Low-level handle to a coroutine frame, used to resume, destroy, or check completion status.

## Pros

- **Readable async code**: Write async code that looks synchronous
- **Low overhead**: Stackless coroutines use heap-allocated frames
- **High scalability**: Can spawn millions of coroutines
- **Customizable**: Promise type allows full control over behavior
- **Exception safety**: Exceptions propagate naturally through coroutines
- **No preemption**: Cooperative multitasking avoids race conditions

## Cons

- **Complexity**: Requires understanding of coroutine mechanics and promise types
- **Compiler support**: Requires C++20-compatible compiler
- **Learning curve**: New keywords and concepts to learn
- **Debugging**: Can be challenging to debug coroutine state machines
- **Frame allocation**: Heap allocation for coroutine frames
- **Limited ecosystem**: Fewer libraries and examples compared to threads

## Underlying Implementation

### Coroutine State Machine

The compiler transforms coroutines into a state machine:

```cpp
// Conceptual coroutine state machine
enum class CoroutineState {
    Created,
    Running,    Suspended,
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

An awaitable object must satisfy these requirements to work with co_await:

```cpp
// Awaitable object must satisfy these requirements
struct Awaiter {
    bool await_ready();
    void await_suspend(std::coroutine_handle<>);
    void await_resume();
};
```

### Standard Awaitables

The standard library provides two built-in awaitables:

```cpp
// Standard library awaitables
struct std::suspend_always {
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};

struct std::suspend_never {
    bool await_ready() noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) noexcept {}
    void await_resume() noexcept {}
};
```

### Frame Layout

Coroutine frames are heap-allocated with a specific layout:

```cpp
// Coroutine frame memory layout
struct CoroutineFrame {
    // Control block
    void* resume_point;
    void* promise_ptr;
    void* destructor;
    
    // Promise object
    Promise promise;
    
    // Local variables
    // (allocated in reverse order of declaration)
    
    // Alignment padding
};
```

### Compiler Transformation

The compiler transforms coroutines into state machines:

```cpp
// Before: Coroutine
Task my_coroutine() {
    int x = 42;
    co_await Awaiter{};
    int y = x + 1;
    co_return y;
}

// After: Compiler transforms to state machine
Task my_coroutine() {
    struct Frame {
        int state = 0;
        int x;
        int y;
        Promise promise;
    };
    
    Frame* frame = new Frame;
    auto handle = std::coroutine_handle<PromiseType>::from_promise(frame->promise);
    
resume:
    switch (frame->state) {
        case 0:
            frame->x = 42;
            frame->state = 1;
            if (!awaiter.await_ready()) {
                awaiter.await_suspend(handle);
                return Task{handle};
            }
        case 1:
            awaiter.await_resume();
            frame->y = frame->x + 1;
            frame->promise.return_value(frame->y);
            frame->state = 2;
            return Task{handle};
        case 2:
            // Completed
            break;
    }
    
    return Task{handle};
}
```

### Handle and Promise

The coroutine handle and promise work together to manage coroutine lifecycle:

```cpp
// Coroutine handle and promise relationship
struct Promise {
    // Allocation
    static void* operator new(std::size_t size) {
        return ::operator new(size);
    }
    
    static void operator delete(void* ptr) {
        ::operator delete(ptr);
    }
    
    // Coroutine lifecycle
    Task get_return_object();
    std::suspend_never initial_suspend();
    std::suspend_never final_suspend() noexcept;
    void return_void();
    void unhandled_exception();
};

struct Task {
    struct promise_type {
        Promise promise;
        // ... promise implementation
    };
    
    std::coroutine_handle<promise_type> handle;
};
```

### Custom Allocator

You can customize coroutine frame allocation for performance:

```cpp
// Custom allocator for coroutine frames
template<typename Allocator>
struct CustomPromise {
    static void* operator new(std::size_t size) {
        Allocator alloc;
        return alloc.allocate(size);
    }
    
    static void operator delete(void* ptr, std::size_t size) {
        Allocator alloc;
        alloc.deallocate(static_cast<char*>(ptr), size);
    }
};
```

### Symmetric Transfer

Symmetric transfer avoids stack growth by directly transferring control:

```cpp
// Symmetric transfer avoids stack growth
void symmetric_transfer(std::coroutine_handle<> from, 
                       std::coroutine_handle<> to) {
    from.destroy();
    to.resume();
}

struct SymmetricAwaiter {
    std::coroutine_handle<> next;
    
    bool await_ready() { return false; }
    
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) {
        return next;  // Direct transfer without stack growth
    }
    
    void await_resume() {}
};
```

### Exception Handling

Coroutines handle exceptions through the promise type:

```cpp
// Exception handling in coroutines
struct SafePromise {
    std::exception_ptr exception;
    
    void unhandled_exception() {
        exception = std::current_exception();
    }
    
    void rethrow_if_exception() {
        if (exception) {
            std::rethrow_exception(exception);
        }
    }
};
```

### Coroutine Lifecycle

Proper lifecycle management prevents memory leaks:

```cpp
// Coroutine lifecycle management
class CoroutineManager {
private:
    std::coroutine_handle<> handle;
    
public:
    CoroutineManager(std::coroutine_handle<> h) : handle(h) {}
    
    ~CoroutineManager() {
        if (handle) handle.destroy();
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
```

### Stackless Nature

Coroutines are stackless, which is why they're efficient:

```cpp
// Coroutines are stackless - no separate call stack
// This is why they're efficient

// Thread stack:
// [ ... ]  // Regular stack frames

// Coroutine frame (heap allocated):
// [ state | promise | locals ]
// Resuming coroutine jumps to resume point
// No stack growth, no context switch
```

## Best Practices

1. **Use RAII for handle management**: Ensure coroutine handles are properly destroyed to avoid memory leaks
2. **Implement proper exception handling**: Catch and propagate exceptions through the promise type
3. **Consider custom allocators**: For performance-critical code with many coroutines
4. **Use symmetric transfer**: Avoid stack growth when switching between coroutines
5. **Monitor frame size**: Large local variables increase coroutine frame memory usage
6. **Use co_return consistently**: Always use co_return instead of regular return in coroutines
7. **Profile allocation overhead**: Coroutine frame allocation can be a bottleneck
8. **Avoid blocking operations**: Coroutines should yield instead of blocking threads
9. **Design reusable promise types**: Create libraries of common coroutine patterns
10. **Test with valgrind/sanitizers**: Catch memory leaks and use-after-free in coroutine frames
