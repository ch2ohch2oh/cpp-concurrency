# Thread-Local Storage: Usage and Implementation

## Practical Usage

### Basic thread_local Variable

Each thread gets its own copy of a `thread_local` variable, initialized on first access per thread. This eliminates the need for synchronization when accessing thread-specific state.

```cpp
thread_local int thread_counter = 0;

void increment() {
    thread_counter++;  // Each thread has its own counter
}
```

**Key points:**
- Variable is initialized separately for each thread
- Changes in one thread don't affect other threads
- Automatic cleanup when thread exits

**See:** `examples/10-thread-local-storage/01-basic-thread-local.cpp`

### thread_local with Classes

`thread_local` can be used with static class members to create thread-specific class state. This is useful for logging, counters, or any per-thread class data.

```cpp
class ThreadLocalLogger {
private:
    thread_local static int instance_count_;
    
public:
    ThreadLocalLogger() {
        instance_count_++;  // Per-thread counter
    }
};

thread_local int ThreadLocalLogger::instance_count_ = 0;
```

**Key points:**
- Use `thread_local static` for class-level thread-specific data
- Each thread has its own instance of the static member
- Constructor runs once per thread on first access

**See:** `examples/10-thread-local-storage/02-thread-local-class.cpp`

### Thread-Specific Random Number Generator

RNGs are expensive to initialize and not thread-safe. Using `thread_local` for RNGs avoids both issues while providing thread-specific random sequences.

```cpp
thread_local std::mt19937 rng(std::random_device{}());

int random_int(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);  // No locking needed
}
```

**Key points:**
- Each thread gets its own RNG instance
- No locking needed for thread-safe random number generation
- Better performance than global RNG with mutex

**See:** `examples/10-thread-local-storage/03-thread-local-rng.cpp`

### Thread-Local Cache

Caches can be made thread-local to avoid synchronization overhead. Each thread maintains its own cache, eliminating contention.

```cpp
class ThreadLocalCache {
private:
    thread_local static std::unordered_map<std::string, int> cache_;
    
public:
    static int get(const std::string& key) {
        // No locks needed - each thread has its own cache
        return cache_[key];
    }
};
```

**Key points:**
- No locks needed for cache access
- Each thread has independent cache entries
- Useful for read-heavy workloads with per-thread computation

**See:** `examples/10-thread-local-storage/04-thread-local-cache.cpp`

### Realistic Example: Thread-Local Database Connection

Database connections are expensive to create. Thread-local connection pools allow each thread to maintain its own connection without synchronization overhead.

```cpp
class ConnectionPool {
private:
    thread_local static DatabaseConnection* connection_;
    
public:
    static DatabaseConnection& get_connection() {
        if (!connection_) {
            connection_ = new DatabaseConnection("localhost:5432");
        }
        return *connection_;  // Lazy initialization per thread
    }
};
```

**Key points:**
- Lazy initialization: connection created on first use per thread
- No contention between threads for connection access
- Must implement cleanup to avoid leaks

**See:** `examples/10-thread-local-storage/05-thread-local-db-connection.cpp`

## Underlying Implementation

### Compiler Support for thread_local

Different compilers implement thread_local differently:

- **GCC/Clang**: Use `__thread` or `thread_local` with TLS model
- **MSVC**: Use `__declspec(thread)` or `thread_local`
- **Platform-specific**: pthread TLS, Windows TLS

### GCC Implementation

GCC uses TLS (Thread Local Storage) segments. Access to thread_local variables is compiled to use the `%fs` or `%gs` segment register on x86_64, which points to the thread's TLS area.

```cpp
thread_local int x = 42;
// Compiled to: mov %fs:TLV_OFFSET(%rip), %eax
```

This makes TLS access fast but slightly slower than regular global variables.

### Platform-Specific TLS

**POSIX (pthread):** Uses `pthread_key_create()`, `pthread_getspecific()`, and `pthread_setspecific()` for dynamic TLS. Requires manual cleanup via destructors.

**Windows:** Uses `TlsAlloc()`, `TlsGetValue()`, and `TlsSetValue()` for TLS management. Similar pattern to pthread but with Windows-specific API.

### TLS Model Options (GCC)

Different TLS models affect performance and code size:

```cpp
thread_local int x;  // global-dynamic (default)

__thread __attribute__((tls_model("local-exec"))) int x;  // fastest
```

- **global-dynamic** (default): Most flexible, works with shared libraries
- **local-dynamic**: Faster for single module, limited to one module
- **initial-exec**: Faster, but limited number of TLS variables
- **local-exec**: Fastest, static executable only

### Memory Layout

Thread-local storage is typically laid out as:

```
High Address
    |
    v
+---------------------------+
| Thread Stack              |  Grows downward
|                           |
|   ... local variables ... |
+---------------------------+
| Stack Guard Page          |
+---------------------------+
| Thread Local Storage (TLS)|  %fs/%gs points here
+---------------------------+  (on x86_64)
| thread_local var 1        |  Offset +0
| thread_local var 2        |  Offset +8
| thread_local var 3        |  Offset +16
| ...                       |
+---------------------------+
| TLS Segment Base Pointer  |
| (points to TLS start)     |
+---------------------------+
| Thread-Specific Data      |
| - Thread ID               |
| - Thread-local errno      |
| - Signal handler mask     |
| - Other runtime data      |
+---------------------------+
| Heap                      |
|                           |
|   ... dynamic alloc ...   |
+---------------------------+
Low Address
```

On x86_64 Linux:
- TLS is accessed via the `%fs` segment register
- The `%fs` base points to the thread's TLS area
- Each thread has its own TLS segment
- `thread_local` variables have fixed offsets from the TLS base
- Example: `mov %fs:0x10, %rax` loads thread_local variable at offset 0x10

On Windows:
- TLS is accessed via the `%gs` segment register
- Each thread has a Thread Environment Block (TEB)
- TLS variables are stored in the TEB
- The TEB also contains thread-specific data likeLastError

The layout ensures that:
- Each thread has isolated storage (no sharing)
- Access is fast (single instruction with segment register)
- No synchronization needed for TLS access
- Variables have deterministic offsets for fast access

### Initialization and Destruction

`thread_local` initialization happens on first access per thread. Destruction happens when the thread exits, with the order being reverse of construction. This is automatic and handled by the runtime.

### Dynamic thread_local

For expensive resources, use `thread_local` with smart pointers like `std::unique_ptr` for lazy initialization.

```cpp
thread_local std::unique_ptr<Resource> resource;

void use_resource() {
    if (!resource) {
        resource = std::make_unique<Resource>();
    }
    resource->do_something();
}
```

This allows you to control when the resource is created while still maintaining thread-local semantics.

### thread_local with Templates

Templates can use `thread_local` static members to create type-specific thread-local singletons.

```cpp
template<typename T>
class ThreadLocalSingleton {
private:
    thread_local static T* instance_;
    
public:
    static T& get() {
        if (!instance_) {
            instance_ = new T();
        }
        return *instance_;
    }
};
```

Each template instantiation gets its own thread-local instance.

### Performance Considerations

- **TLS access**: ~5-10 cycles on x86_64
- **Regular variable**: ~1-2 cycles
- **Mutex-protected access**: ~50-100 cycles

TLS is significantly faster than mutex-based synchronization, though slower than regular variables. The tradeoff is worthwhile when avoiding lock contention.

### TLS Size Limits

TLS has size limits that vary by platform:
- **Linux**: Typically several MB per thread
- **Windows**: Limited by initial TLS size

Be mindful of large thread_local objects as they consume TLS space per thread.

### Exception Safety

`thread_local` initialization can throw (e.g., `std::bad_alloc` for containers). Handle exceptions appropriately when initializing thread_local objects that may fail.

### thread_local in Shared Libraries

Using `thread_local` in shared libraries requires careful handling. Use the global-dynamic TLS model for compatibility across different platforms and shared library scenarios.

### Combining thread_local with atomics

`thread_local` can be combined with other synchronization primitives like `std::atomic` for scenarios where you need both thread-local storage and atomic operations within that thread's context.

```cpp
thread_local std::atomic<int> counter(0);

void increment() {
    counter.fetch_add(1, std::memory_order_relaxed);
}
```

## Best Practices

1. Use `thread_local` for thread-specific state to avoid synchronization
2. Prefer `thread_local` over manual thread-specific maps (e.g., `std::map<std::thread::id, T>`) to avoid lookup overhead and mutex contention
3. Be aware of TLS access overhead (~5-10 cycles) - slower than regular variables but much faster than mutex-protected access (~50-100 cycles)
4. Clean up `thread_local` resources explicitly when needed
5. Use appropriate TLS model for shared libraries
6. Be aware of `thread_local` initialization order within a thread
7. Consider using `thread_local` for expensive per-thread resources (connections, caches)
8. Remember `thread_local` variables are destroyed when thread exits
9. Test on all target platforms (TLS implementation varies)
10. Use `thread_local` for RNG to avoid lock contention
