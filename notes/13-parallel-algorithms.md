# Parallel Algorithms: Usage and Implementation

## Practical Usage

### std::sort with Parallel Execution

```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>
#include <random>

int main() {
    std::vector<int> data(1000000);
    std::mt19937 rng(42);
    std::generate(data.begin(), data.end(), [&]() { return rng(); });
    
    // Sequential sort
    auto start = std::chrono::high_resolution_clock::now();
    std::sort(data.begin(), data.end());
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Sequential: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";
    
    // Parallel sort
    std::generate(data.begin(), data.end(), [&]() { return rng(); });
    start = std::chrono::high_resolution_clock::now();
    std::sort(std::execution::par, data.begin(), data.end());
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Parallel: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";
    
    return 0;
}
```

### std::for_each with Parallel Execution

```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>

void process_item(int& item) {
    item *= 2;
}

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 0);
    
    // Parallel for_each
    std::for_each(std::execution::par, data.begin(), data.end(), process_item);
    
    std::cout << "First element: " << data[0] << "\n";
    std::cout << "Last element: " << data.back() << "\n";
    
    return 0;
}
```

### std::transform with Parallel Execution

```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <cmath>

int main() {
    std::vector<double> input(1000000);
    std::vector<double> output(1000000);
    
    std::iota(input.begin(), input.end(), 0.0);
    
    // Parallel transform
    std::transform(std::execution::par, 
                  input.begin(), input.end(), 
                  output.begin(),
                  [](double x) { return std::sqrt(x); });
    
    return 0;
}
```

### std::reduce (Parallel Reduction)

```cpp
#include <numeric>
#include <execution>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 1);
    
    // Sequential accumulate
    auto sum_seq = std::accumulate(data.begin(), data.end(), 0);
    std::cout << "Sequential sum: " << sum_seq << "\n";
    
    // Parallel reduce
    auto sum_par = std::reduce(std::execution::par, 
                              data.begin(), data.end(), 
                              0, std::plus<>{});
    std::cout << "Parallel sum: " << sum_par << "\n";
    
    return 0;
}
```

### std::count_if with Parallel Execution

```cpp
#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 0);
    
    // Parallel count_if
    auto count = std::count_if(std::execution::par, 
                               data.begin(), data.end(),
                               [](int x) { return x % 2 == 0; });
    
    std::cout << "Even numbers: " << count << "\n";
    
    return 0;
}
```

### Realistic Example: Parallel Image Processing

```cpp
#include <algorithm>
#include <execution>
#include <vector>

struct Pixel {
    unsigned char r, g, b;
};

std::vector<Pixel> apply_grayscale(const std::vector<Pixel>& image) {
    std::vector<Pixel> result(image.size());
    
    std::transform(std::execution::par,
                  image.begin(), image.end(),
                  result.begin(),
                  [](const Pixel& p) {
                      unsigned char gray = static_cast<unsigned char>(
                          0.299 * p.r + 0.587 * p.g + 0.114 * p.b);
                      return Pixel{gray, gray, gray};
                  });
    
    return result;
}
```

### Execution Policies

```cpp
#include <execution>

// seq - Sequential execution (default)
std::sort(std::execution::seq, data.begin(), data.end());

// par - Parallel execution
std::sort(std::execution::par, data.begin(), data.end());

// par_unseq - Parallel + vectorized (SIMD)
std::sort(std::execution::par_unseq, data.begin(), data.end());
```

## Underlying Implementation

### Execution Policy Abstraction

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

1. Profile to determine if parallel execution is beneficial
2. Use par for CPU-bound algorithms with large datasets
3. Use par_unseq when operations are vectorizable
4. Be aware of exception handling in parallel algorithms
5. Ensure operations are thread-safe when using par/par_unseq
6. Consider overhead for small datasets (sequential may be faster)
7. Use appropriate chunk sizes for work distribution
8. Test on target hardware (SIMD support varies)
9. Avoid data races in parallel algorithms
10. Use sequential execution for deterministic ordering requirements
