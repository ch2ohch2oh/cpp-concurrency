# Task-Based Concurrency Patterns: Usage and Implementation

## Practical Usage

### Task-Based Parallelism

```cpp
#include <future>
#include <vector>
#include <numeric>

std::future<int> compute_sum(const std::vector<int>& data) {
    return std::async(std::launch::async, [&data]() {
        return std::accumulate(data.begin(), data.end(), 0);
    });
}

int main() {
    std::vector<int> data1(1000, 1);
    std::vector<int> data2(1000, 2);
    
    auto future1 = compute_sum(data1);
    auto future2 = compute_sum(data2);
    
    int sum = future1.get() + future2.get();
    std::cout << "Total sum: " << sum << "\n";
    
    return 0;
}
```

### Fork-Join Pattern

```cpp
#include <future>
#include <algorithm>

template<typename It, typename F>
auto parallel_fork_join(It first, It last, F func, size_t threshold = 1000)
    -> std::future<decltype(func(first, last))> {
    
    using ReturnType = decltype(func(first, last));
    
    auto size = std::distance(first, last);
    if (size < threshold) {
        std::promise<ReturnType> prom;
        std::future<ReturnType> fut = prom.get_future();
        prom.set_value(func(first, last));
        return fut;
    }
    
    It mid = first + size / 2;
    
    auto left = parallel_fork_join(first, mid, func, threshold);
    auto right = parallel_fork_join(mid, last, func, threshold);
    
    return std::async(std::launch::async, [left = std::move(left), 
                                             right = std::move(right), func]() mutable {
        return func(left.get(), right.get());
    });
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    auto sum_future = parallel_fork_join(
        data.begin(), data.end(),
        [](auto first, auto last) {
            return std::accumulate(first, last, 0);
        }
    );
    
    std::cout << "Sum: " << sum_future.get() << "\n";
    return 0;
}
```

### Pipeline Pattern

```cpp
#include <future>
#include <queue>
#include <thread>

template<typename T>
class PipelineStage {
private:
    std::function<T(T)> transform_;
    std::queue<std::promise<T>> output_queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread thread_;
    bool stop_;
    
public:
    PipelineStage(std::function<T(T)> transform) 
        : transform_(transform), stop_(false) {
        thread_ = std::thread([this] {
            while (true) {
                std::promise<T> prom;
                {
                    std::unique_lock<std::mutex> lock(mtx_);
                    cv_.wait(lock, [this] { 
                        return stop_ || !output_queue_.empty(); 
                    });
                    
                    if (stop_ && output_queue_.empty()) return;
                    
                    prom = std::move(output_queue_.front());
                    output_queue_.pop();
                }
                
                // Process and set value
                // (In real implementation, would get input from previous stage)
            }
        });
    }
    
    ~PipelineStage() {
        stop_ = true;
        cv_.notify_all();
        thread_.join();
    }
};
```

### Map-Reduce Pattern

```cpp
#include <future>
#include <vector>
#include <algorithm>

template<typename InputIt, typename MapFunc, typename ReduceFunc>
auto map_reduce(InputIt first, InputIt last, 
                MapFunc map, ReduceFunc reduce,
                size_t chunk_size = 1000) 
    -> typename std::result_of<MapFunc decltype(*first)>::type {
    
    using MappedType = typename std::result_of<MapFunc decltype(*first)>::type;
    
    std::vector<std::future<MappedType>> futures;
    
    for (auto it = first; it < last; it += chunk_size) {
        auto end = std::min(it + chunk_size, last);
        
        futures.push_back(std::async(std::launch::async, [=]() {
            MappedType result = map(*it);
            for (auto i = it + 1; i != end; ++i) {
                result = reduce(result, map(*i));
            }
            return result;
        }));
    }
    
    MappedType final_result = futures[0].get();
    for (size_t i = 1; i < futures.size(); ++i) {
        final_result = reduce(final_result, futures[i].get());
    }
    
    return final_result;
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    auto result = map_reduce(
        data.begin(), data.end(),
        [](int x) { return x * x; },  // Map
        [](int a, int b) { return a + b; }  // Reduce
    );
    
    std::cout << "Sum of squares: " << result << "\n";
    return 0;
}
```

### Divide and Conquer Pattern

```cpp
#include <future>
#include <algorithm>

template<typename It>
void parallel_quick_sort(It first, It last) {
    auto size = std::distance(first, last);
    
    if (size < 1000) {
        std::sort(first, last);
        return;
    }
    
    auto pivot = *std::next(first, size / 2);
    auto mid1 = std::partition(first, last, [pivot](const auto& x) { return x < pivot; });
    auto mid2 = std::partition(mid1, last, [pivot](const auto& x) { return !(pivot < x); });
    
    auto left = std::async(std::launch::async, [=] {
        parallel_quick_sort(first, mid1);
    });
    
    parallel_quick_sort(mid2, last);
    
    left.wait();
}
```

### Work Stealing Queue

```cpp
#include <deque>
#include <mutex>
#include <thread>

template<typename T>
class WorkStealingQueue {
private:
    std::deque<T> queue_;
    std::mutex mtx_;
    
public:
    void push(T task) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push_back(std::move(task));
    }
    
    bool try_pop(T& task) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        task = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }
    
    bool try_steal(T& task) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (queue_.empty()) return false;
        task = std::move(queue_.back());
        queue_.pop_back();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }
};
```

### Realistic Example: Parallel File Processing

```cpp
#include <future>
#include <vector>
#include <string>
#include <fstream>

struct FileResult {
    std::string filename;
    size_t line_count;
    size_t word_count;
};

std::future<FileResult> process_file_async(const std::string& filename) {
    return std::async(std::launch::async, [filename]() {
        FileResult result{filename, 0, 0};
        std::ifstream file(filename);
        
        std::string line;
        while (std::getline(file, line)) {
            result.line_count++;
            std::istringstream iss(line);
            std::string word;
            while (iss >> word) {
                result.word_count++;
            }
        }
        
        return result;
    });
}

std::vector<FileResult> process_files(const std::vector<std::string>& filenames) {
    std::vector<std::future<FileResult>> futures;
    
    for (const auto& filename : filenames) {
        futures.push_back(process_file_async(filename));
    }
    
    std::vector<FileResult> results;
    for (auto& fut : futures) {
        results.push_back(fut.get());
    }
    
    return results;
}
```

## Underlying Implementation

### Task Scheduler

```cpp
class TaskScheduler {
private:
    struct Task {
        std::function<void()> func;
        std::vector<std::shared_ptr<Task>> dependencies;
        std::atomic<int> pending_deps;
        bool completed = false;
    };
    
    std::queue<std::shared_ptr<Task>> ready_queue_;
    std::unordered_map<size_t, std::shared_ptr<Task>> all_tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    
public:
    TaskScheduler(size_t num_workers) : stop_(false) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }
    
    void worker_loop() {
        while (!stop_) {
            std::shared_ptr<Task> task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this] { 
                    return stop_ || !ready_queue_.empty(); 
                });
                
                if (stop_ && ready_queue_.empty()) return;
                
                task = std::move(ready_queue_.front());
                ready_queue_.pop();
            }
            
            task->func();
            task->completed = true;
            
            // Check dependent tasks
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto& dep_task : all_tasks_) {
                for (auto& dependency : dep_task.second->dependencies) {
                    if (dependency == task) {
                        if (--dep_task.second->pending_deps == 0) {
                            ready_queue_.push(dep_task.second);
                            cv_.notify_one();
                        }
                    }
                }
            }
        }
    }
    
    size_t schedule(std::function<void()> func,
                   const std::vector<size_t>& dependency_ids = {}) {
        static std::atomic<size_t> next_id{0};
        size_t task_id = next_id++;
        
        auto task = std::make_shared<Task>();
        task->func = func;
        task->pending_deps = dependency_ids.size();
        
        std::lock_guard<std::mutex> lock(mtx_);
        all_tasks_[task_id] = task;
        
        if (dependency_ids.empty()) {
            ready_queue_.push(task);
            cv_.notify_one();
        }
        
        return task_id;
    }
    
    ~TaskScheduler() {
        stop_ = true;
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Continuation Passing Style

```cpp
template<typename T>
class Task {
private:
    std::shared_ptr<std::promise<T>> promise_;
    std::function<void()> continuation_;
    
public:
    Task() : promise_(std::make_shared<std::promise<T>>()) {}
    
    std::future<T> get_future() {
        return promise_->get_future();
    }
    
    template<typename F>
    auto then(F&& func) -> Task<typename std::result_of<F(T)>::type> {
        using ResultType = typename std::result_of<F(T)>::type;
        
        Task<ResultType> next_task;
        
        continuation_ = [this, func, next_task]() mutable {
            try {
                T result = promise_->get_future().get();
                ResultType next_result = func(result);
                next_task.promise_->set_value(next_result);
            } catch (...) {
                next_task.promise_->set_exception(std::current_exception());
            }
        };
        
        return next_task;
    }
    
    void set_value(T value) {
        promise_->set_value(value);
        if (continuation_) {
            continuation_();
        }
    }
};
```

### Task Graph Executor

```cpp
class TaskGraphExecutor {
private:
    struct TaskNode {
        std::function<void()> task;
        std::vector<std::shared_ptr<TaskNode>> successors;
        std::atomic<int> unfinished_predecessors;
    };
    
    std::vector<std::shared_ptr<TaskNode>> task_nodes_;
    std::vector<std::thread> workers_;
    std::queue<std::shared_ptr<TaskNode>> ready_tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    
public:
    TaskGraphExecutor(size_t num_workers) : stop_(false) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }
    
    void worker_loop() {
        while (!stop_) {
            std::shared_ptr<TaskNode> node;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this] { 
                    return stop_ || !ready_tasks_.empty(); 
                });
                
                if (stop_ && ready_tasks_.empty()) return;
                
                node = std::move(ready_tasks_.front());
                ready_tasks_.pop();
            }
            
            node->task();
            
            for (auto& successor : node->successors) {
                if (--successor->unfinished_predecessors == 0) {
                    std::lock_guard<std::mutex> lock(mtx_);
                    ready_tasks_.push(successor);
                    cv_.notify_one();
                }
            }
        }
    }
    
    void add_task(std::function<void()> task,
                  const std::vector<size_t>& predecessor_ids = {}) {
        auto node = std::make_shared<TaskNode>();
        node->task = task;
        node->unfinished_predecessors = predecessor_ids.size();
        
        for (size_t pred_id : predecessor_ids) {
            task_nodes_[pred_id]->successors.push_back(node);
        }
        
        task_nodes_.push_back(node);
        
        if (predecessor_ids.empty()) {
            std::lock_guard<std::mutex> lock(mtx_);
            ready_tasks_.push(node);
            cv_.notify_one();
        }
    }
    
    void wait_for_completion() {
        for (auto& node : task_nodes_) {
            while (node->unfinished_predecessors > 0) {
                std::this_thread::yield();
            }
        }
    }
    
    ~TaskGraphExecutor() {
        stop_ = true;
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Task Affinity

```cpp
class AffinityAwareScheduler {
private:
    struct Worker {
        std::thread thread;
        std::queue<std::function<void()>> local_queue;
        std::mutex mtx;
        int cpu_id;
    };
    
    std::vector<Worker> workers_;
    std::atomic<size_t> next_worker_;
    
public:
    AffinityAwareScheduler(size_t num_workers) : next_worker_(0) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.push_back({{}, {}, {}, static_cast<int>(i)});
            workers_[i].thread = std::thread([this, i] { worker_loop(i); });
            
            // Set CPU affinity
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            pthread_setaffinity_np(workers_[i].thread.native_handle(),
                                  sizeof(cpu_set_t), &cpuset);
        }
    }
    
    void worker_loop(size_t worker_id) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(workers_[worker_id].mtx);
                if (workers_[worker_id].local_queue.empty()) {
                    // Try to steal from other workers
                    task = steal_task(worker_id);
                    if (!task) {
                        std::this_thread::yield();
                        continue;
                    }
                } else {
                    task = std::move(workers_[worker_id].local_queue.front());
                    workers_[worker_id].local_queue.pop();
                }
            }
            
            task();
        }
    }
    
    std::function<void()> steal_task(size_t thief_id) {
        for (size_t i = 1; i < workers_.size(); ++i) {
            size_t victim = (thief_id + i) % workers_.size();
            
            std::lock_guard<std::mutex> lock(workers_[victim].mtx);
            if (!workers_[victim].local_queue.empty()) {
                auto task = std::move(workers_[victim].local_queue.front());
                workers_[victim].local_queue.pop();
                return task;
            }
        }
        return nullptr;
    }
};
```

## Best Practices

1. Prefer task-based over thread-based concurrency
2. Use appropriate granularity for tasks (not too small, not too large)
3. Implement work stealing for load balancing
4. Handle task dependencies carefully to avoid deadlocks
5. Use continuations for async composition
6. Consider task affinity for cache locality
7. Monitor task queue lengths for performance tuning
8. Use exception-safe task execution
9. Implement task cancellation when needed
10. Profile to find optimal task chunk sizes
