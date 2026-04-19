# Parallel Algorithms

## Motivation

C++17 introduced parallel algorithms to leverage multi-core processors without requiring manual thread management. Before C++17, developers had to manually parallelize algorithms using threads, which was error-prone and complex. Parallel algorithms provide a simple interface: just pass an execution policy to standard algorithms like `std::sort`, `std::transform`, or `std::reduce`.

The key motivation is to make parallelism accessible and safe. By using standard library implementations, you get:
- **Simplicity**: No manual thread management or synchronization
- **Performance**: Optimized implementations that use thread pools and work stealing
- **Safety**: Avoids common concurrency bugs like data races and deadlocks
- **Portability**: Works across different platforms and hardware configurations

## Execution Policies

C++17 provides three execution policies:

- **`std::execution::seq`**: Sequential execution (default behavior)
- **`std::execution::par`**: Parallel execution across multiple threads
- **`std::execution::par_unseq`**: Parallel execution with vectorization (SIMD)

```cpp
#include <execution>

// Sequential execution
std::sort(std::execution::seq, data.begin(), data.end());

// Parallel execution
std::sort(std::execution::par, data.begin(), data.end());

// Parallel + vectorized (SIMD)
std::sort(std::execution::par_unseq, data.begin(), data.end());
```

## Practical Usage

See the `examples/parallel-algorithms/` directory for complete working examples:
- `01-sort.cpp` - Comparing sequential vs parallel sort performance
- `02-for-each.cpp` - Parallel iteration with `std::for_each`
- `03-transform.cpp` - Parallel element-wise transformations
- `04-reduce.cpp` - Parallel reduction operations
- `05-count-if.cpp` - Parallel conditional counting

### Common Parallel Algorithms

**std::sort**: Sorts elements in parallel using divide-and-conquer. Best for large datasets (typically >10,000 elements).

**std::for_each**: Applies a function to each element in parallel. Useful for independent operations.

**std::transform**: Creates a new range by applying a function. Ideal for element-wise computations.

**std::reduce**: Parallel reduction (alternative to `std::accumulate`). Essential for summing, finding min/max, etc.

**std::count_if**: Counts elements matching a predicate in parallel.

## Pros

- **Ease of use**: Simply add an execution policy to existing algorithm calls
- **Performance**: Automatically utilizes available CPU cores
- **Standard library integration**: Works with familiar algorithms
- **No manual synchronization**: Library handles thread safety
- **Work stealing**: Many implementations use efficient work-stealing schedulers
- **Scalability**: Scales with hardware concurrency

## Cons

- **Overhead**: Thread creation and coordination adds overhead for small datasets
- **Non-deterministic**: Execution order is not guaranteed (except for seq)
- **Exception handling**: Exceptions in parallel algorithms can be complex
- **Data race risks**: Operations must be thread-safe when using par/par_unseq
- **Limited algorithms**: Not all algorithms support parallel execution
- **Compiler support**: Requires C++17 and proper library support (e.g., TBB)

## When to Use Parallel Algorithms

Parallel algorithms are most effective when:
- **Large datasets**: Typically >10,000 elements to amortize thread overhead
- **CPU-bound operations**: Computations that benefit from parallel execution
- **Independent operations**: Elements can be processed without dependencies
- **Embarrassingly parallel**: No shared state or complex synchronization needed

Avoid parallel algorithms when:
- **Small datasets**: Overhead outweighs benefits
- **I/O-bound operations**: Parallelism won't help with disk/network bottlenecks
- **Complex dependencies**: Require manual synchronization
- **Strict ordering requirements**: Need deterministic execution order

## Key Considerations

**Thread Safety**: Operations must be thread-safe when using `par` or `par_unseq`. For example, modifying shared state in a lambda passed to `std::for_each` will cause data races.

**Exception Handling**: If an exception is thrown in any parallel task, the algorithm terminates and the exception is rethrown in the calling thread. Other tasks may continue running briefly before termination.

**Memory Ordering**: Parallel algorithms respect the memory ordering semantics of the operations they perform. For `par_unseq`, additional restrictions apply to ensure vectorization safety.

**Performance Characteristics**: The actual performance gain depends on:
- Number of CPU cores available
- Granularity of work (chunk size)
- Cache locality and memory bandwidth
- Nature of the algorithm (compute vs memory bound)

## Underlying Implementation

The implementation of parallel algorithms varies by compiler and library, but generally follows these patterns:

### Execution Policy Abstraction

Execution policies are tag types that select the algorithm implementation at compile time:

```cpp
namespace std {
    namespace execution {
        // Sequential execution policy
        struct sequenced_policy {};
        
        // Parallel execution policy
        struct parallel_policy {};
        
        // Parallel unsequenced (vectorized) policy
        struct parallel_unsequenced_policy {};
        
        inline constexpr sequenced_policy seq{};
        inline constexpr parallel_policy par{};
        inline constexpr parallel_unsequenced_policy par_unseq{};
    }
}
```

### Parallel Sort Implementation

Parallel sort typically uses a divide-and-conquer approach:

```cpp
// Conceptual implementation of parallel sort
template<typename ExecutionPolicy, typename RandomIt>
void sort(ExecutionPolicy&& policy, RandomIt first, RandomIt last) {
    if constexpr (is_sequential_v<ExecutionPolicy>) {
        // Use sequential quicksort or introsort
        sequential_sort(first, last);
    } else {
        // Parallel divide-and-conquer
        parallel_quick_sort(first, last);
    }
}

template<typename RandomIt>
void parallel_quick_sort(RandomIt first, RandomIt last) {
    auto size = std::distance(first, last);
    
    if (size < threshold) {
        // Sequential sort for small ranges
        std::sort(first, last);
        return;
    }
    
    // Partition
    auto pivot = partition(first, last);
    
    // Spawn parallel tasks for left and right partitions
    std::future<void> left = std::async(std::launch::async, [=]() {
        parallel_quick_sort(first, pivot);
    });
    
    parallel_quick_sort(pivot + 1, last);
    
    left.wait();
}
```

### Thread Pool for Parallel Algorithms

Most implementations use a thread pool to avoid the overhead of creating threads for each algorithm call:

```cpp
class ParallelThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ParallelThreadPool(size_t threads) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }
    
    ~ParallelThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Parallel For Each Implementation

Parallel `for_each` divides the range into chunks and processes each chunk in a separate thread:

```cpp
template<typename ExecutionPolicy, typename ForwardIt, typename UnaryOp>
void for_each(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last, UnaryOp op) {
    auto size = std::distance(first, last);
    const size_t chunk_size = 1000;  // Tunable chunk size
    
    if constexpr (is_sequential_v<ExecutionPolicy>) {
        for (auto it = first; it != last; ++it) {
            op(*it);
        }
    } else {
        std::vector<std::future<void>> futures;
        
        for (auto it = first; it < last; it += chunk_size) {
            auto end = std::min(it + chunk_size, last);
            
            futures.push_back(std::async(std::launch::async, [=]() {
                for (auto i = it; i != end; ++i) {
                    op(*i);
                }
            }));
        }
        
        for (auto& f : futures) {
            f.wait();
        }
    }
}
```

### Parallel Reduce Implementation

Parallel reduce uses a divide-and-conquer strategy to compute results in parallel:

```cpp
template<typename ExecutionPolicy, typename ForwardIt, typename T, typename BinaryOp>
T reduce(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last, T init, BinaryOp op) {
    auto size = std::distance(first, last);
    
    if constexpr (is_sequential_v<ExecutionPolicy>) {
        return std::accumulate(first, last, init, op);
    } else {
        const size_t num_chunks = std::thread::hardware_concurrency();
        const size_t chunk_size = (size + num_chunks - 1) / num_chunks;
        
        std::vector<std::future<T>> futures;
        std::vector<T> partial_results(num_chunks);
        
        for (size_t i = 0; i < num_chunks; ++i) {
            auto chunk_start = first + i * chunk_size;
            auto chunk_end = std::min(chunk_start + chunk_size, last);
            
            if (chunk_start >= last) break;
            
            futures.push_back(std::async(std::launch::async, [=, &partial_results]() {
                T partial = init;
                for (auto it = chunk_start; it != chunk_end; ++it) {
                    partial = op(partial, *it);
                }
                return partial;
            }));
        }
        
        // Combine partial results
        T result = init;
        for (size_t i = 0; i < futures.size(); ++i) {
            result = op(result, futures[i].get());
        }
        
        return result;
    }
}
```

### Work Stealing Scheduler

Advanced implementations use work stealing to balance load across threads:

```cpp
class WorkStealingScheduler {
private:
    struct Worker {
        std::deque<std::function<void()>> local_queue;
        std::mutex mtx;
    };
    
    std::vector<Worker> workers_;
    std::atomic<size_t> next_worker_;
    
public:
    WorkStealingScheduler(size_t num_workers) : workers_(num_workers), next_worker_(0) {}
    
    void submit(size_t worker_id, std::function<void()> task) {
        std::lock_guard<std::mutex> lock(workers_[worker_id].mtx);
        workers_[worker_id].local_queue.push_back(std::move(task));
    }
    
    std::function<void()> steal_or_pop(size_t worker_id) {
        // Try to steal from other workers
        for (size_t i = 1; i < workers_.size(); ++i) {
            size_t victim = (worker_id + i) % workers_.size();
            
            std::lock_guard<std::mutex> lock(workers_[victim].mtx);
            if (!workers_[victim].local_queue.empty()) {
                auto task = std::move(workers_[victim].local_queue.back());
                workers_[victim].local_queue.pop_back();
                return task;
            }
        }
        
        // Pop from own queue
        std::lock_guard<std::mutex> lock(workers_[worker_id].mtx);
        if (!workers_[worker_id].local_queue.empty()) {
            auto task = std::move(workers_[worker_id].local_queue.front());
            workers_[worker_id].local_queue.pop_front();
            return task;
        }
        
        return nullptr;
    }
};
```

### Vectorization Support (par_unseq)

The `par_unseq` policy enables SIMD vectorization for additional performance:

```cpp
// par_unseq enables SIMD vectorization
template<typename ExecutionPolicy, typename ForwardIt, typename UnaryOp>
void transform(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last, ForwardIt d_first, UnaryOp op) {
    if constexpr (is_parallel_unsequenced_v<ExecutionPolicy>) {
        // Can use SIMD instructions
        // Compiler may auto-vectorize
        #pragma omp simd
        for (auto it = first; it != last; ++it, ++d_first) {
            *d_first = op(*it);
        }
    } else if constexpr (is_parallel_v<ExecutionPolicy>) {
        // Parallel but not vectorized
        parallel_transform(first, last, d_first, op);
    } else {
        // Sequential
        std::transform(first, last, d_first, op);
    }
}
```

### Exception Handling in Parallel Algorithms

Parallel algorithms must handle exceptions carefully to ensure proper cleanup:

```cpp
template<typename ExecutionPolicy, typename ForwardIt, typename UnaryOp>
void for_each(ExecutionPolicy&& policy, ForwardIt first, ForwardIt last, UnaryOp op) {
    std::vector<std::exception_ptr> exceptions;
    std::mutex exceptions_mtx;
    
    auto safe_op = [&](auto&&... args) {
        try {
            op(std::forward<decltype(args)>(args)...);
        } catch (...) {
            std::lock_guard<std::mutex> lock(exceptions_mtx);
            exceptions.push_back(std::current_exception());
        }
    };
    
    // Execute with safe_op
    parallel_for_each(policy, first, last, safe_op);
    
    // Re-throw first exception
    if (!exceptions.empty()) {
        std::rethrow_exception(exceptions[0]);
    }
}
```

### Load Balancing

Dynamic load balancing ensures all threads stay busy:

```cpp
// Dynamic load balancing
template<typename ForwardIt, typename UnaryOp>
void balanced_parallel_for(ForwardIt first, ForwardIt last, UnaryOp op) {
    const size_t num_threads = std::thread::hardware_concurrency();
    std::atomic<size_t> next_index{0};
    const size_t total = std::distance(first, last);
    
    auto worker = [&]() {
        while (true) {
            size_t index = next_index.fetch_add(1);
            if (index >= total) break;
            
            op(*(first + index));
        }
    };
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
}
```

## Best Practices

1. **Profile first**: Measure performance before and after adding parallel execution
2. **Use par for CPU-bound work**: Best for compute-intensive operations on large datasets
3. **Use par_unseq when vectorizable**: Only when operations are SIMD-friendly and have no side effects
4. **Handle exceptions properly**: Be aware that exceptions in parallel tasks are rethrown in the calling thread
5. **Ensure thread safety**: Operations must not cause data races when using par/par_unseq
6. **Consider overhead**: Sequential may be faster for small datasets (<10,000 elements)
7. **Tune chunk sizes**: Optimal chunk size depends on workload and hardware
8. **Test on target hardware**: SIMD support and performance characteristics vary
9. **Avoid shared state**: Prefer pure functions without side effects
10. **Use seq for ordering**: When deterministic execution order is required
