# std::thread: Usage and Implementation

## Practical Usage

### Basic Thread Creation

```cpp
#include <thread>
#include <iostream>

void worker_function(int id) {
    std::cout << "Worker " << id << " is running\n";
}

int main() {
    // Create a thread that runs worker_function
    std::thread t1(worker_function, 1);
    
    // Join the thread (wait for it to finish)
    t1.join();
    
    return 0;
}
```

### Thread with Lambda

```cpp
#include <thread>
#include <iostream>

int main() {
    int value = 42;
    
    std::thread t([value]() {
        std::cout << "Lambda thread, value: " << value << "\n";
    });
    
    t.join();
    return 0;
}
```

### Detaching Threads

```cpp
#include <thread>
#include <iostream>

void background_task() {
    std::cout << "Background task running\n";
}

int main() {
    std::thread t(background_task);
    t.detach();  // Thread runs independently
    
    // Must ensure main doesn't exit before detached thread finishes
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

### Moving Threads

```cpp
#include <thread>
#include <utility>

void task() {}

int main() {
    std::thread t1(task);
    std::thread t2 = std::move(t1);  // t1 is now empty
    
    // Check if t2 owns an active thread before joining (t1 is empty after move)
    if (t2.joinable()) {
        t2.join();
    }
    return 0;
}
```

### Realistic Example: Parallel File Processing

```cpp
#include <thread>
#include <vector>
#include <string>
#include <iostream>

void process_file(const std::string& filename, int thread_id) {
    std::cout << "Thread " << thread_id << " processing " << filename << "\n";
    // Simulate file processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

int main() {
    std::vector<std::string> files = {"file1.txt", "file2.txt", "file3.txt", "file4.txt"};
    std::vector<std::thread> threads;
    
    for (size_t i = 0; i < files.size(); ++i) {
        threads.emplace_back(process_file, files[i], i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### Thread ID and Hardware Concurrency

```cpp
#include <thread>
#include <iostream>

int main() {
    std::cout << "Hardware concurrency: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "Main thread ID: " << std::this_thread::get_id() << "\n";
    
    std::thread t([]() {
        std::cout << "Worker thread ID: " << std::this_thread::get_id() << "\n";
    });
    
    t.join();
    return 0;
}
```

## Underlying Implementation

### OS-Level Threading

std::thread is a thin wrapper around the operating system's native threading API:

- **Linux/macOS**: Uses pthread (POSIX threads)
- **Windows**: Uses Win32 threads
- **Other platforms**: Platform-specific threading APIs

### Typical Implementation Structure

```cpp
// Simplified conceptual implementation
namespace std {
    class thread {
    public:
        using native_handle_type = /* platform-specific type */;
        
        thread() noexcept : id_(0) {}
        
        template<class Function, class... Args>
        explicit thread(Function&& f, Args&&... args) {
            // Create native thread
            start_thread(std::forward<Function>(f), 
                        std::forward<Args>(args)...);
        }
        
        ~thread() {
            if (joinable()) {
                std::terminate();  // Must join or detach before destruction
            }
        }
        
        bool joinable() const noexcept { return id_ != 0; }
        
        void join() {
            if (!joinable()) throw std::system_error(/*...*/);
            // Call OS join function
            native_join();
            id_ = 0;
        }
        
        void detach() {
            if (!joinable()) throw std::system_error(/*...*/);
            // Call OS detach function
            native_detach();
            id_ = 0;
        }
        
        id get_id() const noexcept { return id_; }
        native_handle_type native_handle() { return handle_; }
        
    private:
        id id_;              // Thread identifier
        native_handle_type handle_;  // OS-level handle
    };
}
```

### POSIX Thread Implementation (Linux/macOS)

```cpp
// Conceptual pthread implementation
#include <pthread.h>

void* thread_wrapper(void* arg) {
    // Extract function and arguments
    // Call the actual thread function
    // Handle exceptions
    return nullptr;
}

void start_thread(/* function, args */) {
    pthread_t pthread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // Create thread with wrapper function
    pthread_create(&pthread, &attr, thread_wrapper, /* args */);
    
    // Store pthread handle
    handle_ = pthread;
    
    pthread_attr_destroy(&attr);
}

void native_join() {
    pthread_join(handle_, nullptr);
}

void native_detach() {
    pthread_detach(handle_);
}
```

### Win32 Thread Implementation (Windows)

```cpp
// Conceptual Win32 implementation
#include <windows.h>

DWORD WINAPI thread_wrapper(LPVOID lpParam) {
    // Extract function and arguments
    // Call the actual thread function
    // Handle exceptions
    return 0;
}

void start_thread(/* function, args */) {
    handle_ = CreateThread(
        nullptr,                   // Default security attributes
        0,                         // Default stack size
        thread_wrapper,            // Thread function
        /* args */,                // Arguments
        0,                         // Default creation flags
        &thread_id_               // Thread identifier
    );
}

void native_join() {
    WaitForSingleObject(handle_, INFINITE);
    CloseHandle(handle_);
}

void native_detach() {
    CloseHandle(handle_);
}
```

### Thread Local Storage Implementation

Thread-local storage is typically implemented using:

- **POSIX**: `pthread_key_create`, `pthread_getspecific`, `pthread_setspecific`
- **Windows**: `TlsAlloc`, `TlsGetValue`, `TlsSetValue`
- **Compiler support**: `__thread` (GCC/Clang) or `__declspec(thread)` (MSVC)

```cpp
// Conceptual thread_local implementation
thread_local int thread_specific_data = 0;

// Compiler generates:
// 1. A TLS key for each thread_local variable
// 2. Access through thread-specific storage lookup
// 3. Initialization on first access per thread
```

### Memory Model Considerations

The implementation must respect the C++ memory model:

- Threads have separate execution contexts
- Shared memory access requires synchronization
- The implementation ensures proper memory barriers for thread operations

```cpp
// Thread creation implies memory synchronization
std::thread t(func);  
// All writes before this point are visible to the new thread

t.join();
// All writes from the thread are visible after join returns
```

### Practical Memory Model Guidance

For most practical use, you don't need to worry about the memory model - proper synchronization primitives handle it automatically.

**When you DO need to think about it:**
- Using `std::atomic` with different memory orderings (relaxed, acquire, release, etc.)
- Lock-free algorithms
- Low-level optimization where default synchronization is too expensive

**When you DON'T need to worry:**
- Using `std::mutex` / `std::lock_guard` / `std::unique_lock`
- Using `std::condition_variable`
- Using `std::future` and `std::promise`
- Using `std::atomic` with default memory ordering (sequentially consistent)
- Basic thread joining (`t.join()` provides synchronization)

```cpp
// Safe without thinking about memory model
std::mutex mtx;
int shared_data = 0;

// Thread 1
{
    std::lock_guard<std::mutex> lock(mtx);
    shared_data = 42;
}

// Thread 2
{
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << shared_data;  // Safe, mutex handles memory model
}
```

The memory model guarantees that mutex-protected data is correctly synchronized across threads. Only when doing advanced lock-free programming or needing extreme performance optimization do you need to understand the memory model deeply.

### Exception Handling in Threads

Exceptions thrown in threads cannot propagate to the creating thread:

```cpp
// Implementation handles exceptions in thread wrapper
void* thread_wrapper(void* arg) {
    try {
        // Call user function
        user_function();
    } catch (...) {
        // Store exception for std::future or call std::terminate
        std::terminate();  // Default behavior for detached threads
    }
    return nullptr;
}
```

**What this means in practice:**

If an exception is thrown inside a thread's function, it cannot be caught by `try/catch` blocks in the creating thread. Each thread has its own exception handling context.

```cpp
#include <thread>
#include <iostream>

void worker() {
    throw std::runtime_error("Error in worker thread");  // This exception
}

int main() {
    try {
        std::thread t(worker);
        t.join();
    } catch (const std::runtime_error& e) {
        // This catch block will NOT execute
        // The exception from worker() doesn't propagate here
        std::cout << "Caught: " << e.what() << "\n";
    }
    return 0;
}
```

If an exception escapes the thread function, `std::terminate()` is called and the program crashes.

**How to handle exceptions in threads:**

**Method 1: Catch exceptions inside the thread function:**
```cpp
void worker() {
    try {
        // Do work that might throw
        throw std::runtime_error("Something went wrong");
    } catch (const std::exception& e) {
        std::cerr << "Thread caught: " << e.what() << "\n";
    }
}
```

**Method 2: Use std::future/std::promise (exceptions propagate through the future):**
```cpp
#include <thread>
#include <future>
#include <iostream>

void worker(std::promise<void> p) {
    try {
        // Do work that might throw
        throw std::runtime_error("Something went wrong");
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<void> p;
    std::future<void> f = p.get_future();

    std::thread t(worker, std::move(p));
    t.detach();

    try {
        f.get();  // Will re-throw the exception here
    } catch (const std::exception& e) {
        std::cout << "Main caught: " << e.what() << "\n";
    }
    return 0;
}
```

### Graceful Thread Termination

In C++, there's no direct way to forcefully terminate a thread from another thread. You must use **cooperative cancellation** - the thread checks a flag and exits gracefully.

#### Method 1: Atomic Flag (C++11+)

```cpp
#include <thread>
#include <atomic>
#include <iostream>

std::atomic<bool> stop_flag{false};

void worker() {
    while (!stop_flag.load()) {
        // Do work
        std::cout << "Working...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Thread exiting gracefully\n";
}

int main() {
    std::thread t(worker);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    stop_flag = true;  // Signal thread to stop
    t.join();          // Wait for it to finish
    return 0;
}
```

#### Method 2: Condition Variable

```cpp
std::mutex mtx;
std::condition_variable cv;
bool stop = false;

void worker() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ return stop; });
    // Thread exits when signaled
}
```

#### Method 3: std::jthread (C++20) - Built-in Cancellation

```cpp
#include <thread>
#include <stop_token>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // Do work
    }
}

int main() {
    std::jthread jt(worker);
    jt.request_stop();  // Gracefully cancel
    jt.join();
    return 0;
}
```

#### Detached Thread Considerations

Detached threads are trickier because you can't `join()` them. They must terminate on their own before the program exits.

```cpp
#include <thread>
#include <atomic>
#include <iostream>

std::atomic<bool> global_stop{false};

void background_worker() {
    while (!global_stop.load()) {
        std::cout << "Background task running\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Background thread exiting\n";
}

int main() {
    std::thread t(background_worker);
    t.detach();  // Thread runs independently
    
    // Main does other work...
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Signal detached thread to stop
    global_stop = true;
    
    // Give detached thread time to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return 0;  // Program exits, detached thread should be done
}
```

**Key points for detached threads:**
- Must be self-terminating - no one can wait for them
- Don't use stack-local resources - detached thread outlives creating scope
- Use static/global flags for shutdown signaling
- Avoid `detach()` when possible - prefer `std::jthread` or regular threads with `join()`

### Stack Size Management

Threads have their own stack space:

- Default stack size varies by platform (typically 1-8MB)
- Can be controlled via platform-specific attributes
- Stack overflow causes undefined behavior

```cpp
// POSIX stack size control
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setstacksize(&attr, 1024 * 1024);  // 1MB stack
pthread_create(&pthread, &attr, thread_func, nullptr);
```

### Thread Scheduling

The OS scheduler handles thread execution:

- Preemptive multitasking: OS switches between threads
- Priority levels affect scheduling order
- Thread affinity can pin threads to specific CPUs

```cpp
// CPU affinity (Linux)
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(0, &cpuset);  // Pin to CPU 0
pthread_setaffinity_np(pthread, sizeof(cpu_set_t), &cpuset);
```

### Performance Considerations

- Thread creation has overhead (~10-50 microseconds)
- Context switching overhead (~1-10 microseconds)
- Thread pooling recommended for short-lived tasks
- Number of threads should not exceed CPU cores for CPU-bound work

## Best Practices

1. Always check `joinable()` before `join()` or `detach()`
2. Ensure threads are joined or detached before destruction
3. Use RAII wrappers for automatic thread management
4. Prefer thread pools for many short-lived tasks
5. Be aware of the number of hardware cores
6. Handle exceptions properly in thread functions
7. Use `std::jthread` (C++20) for automatic cancellation support
