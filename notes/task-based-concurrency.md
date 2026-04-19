# Task-Based Concurrency Patterns

## Motivation

Task-based concurrency focuses on defining units of work (tasks) rather than managing threads directly. This paradigm shifts from thinking about "which thread does what" to "what work needs to be done," allowing the runtime or thread pool to handle scheduling and execution.

The key motivation is to write more maintainable and scalable concurrent code:
- **Abstraction**: Separates work definition from execution mechanism
- **Composability**: Tasks can be combined into larger workflows
- **Efficiency**: Runtime can optimize scheduling based on available resources
- **Safety**: Reduces common threading bugs like race conditions and deadlocks
- **Scalability**: Automatically adapts to hardware concurrency

Task-based concurrency is particularly useful for:
- Parallel algorithms (divide and conquer, map-reduce)
- Asynchronous I/O operations
- Pipeline processing
- Complex dependency graphs

## Practical Usage

See the `examples/task-based-concurrency/` directory for complete working examples:
- `01-task-based-parallelism.cpp` - Basic task-based parallelism with futures
- `02-fork-join.cpp` - Recursive fork-join pattern for parallel algorithms
- `03-map-reduce.cpp` - Map-reduce pattern for parallel aggregation

### Common Patterns

**Task-Based Parallelism**: Use `std::async` to launch independent tasks and collect results via futures. Ideal for embarrassingly parallel problems.

**Fork-Join**: Recursively split work into subtasks, execute them in parallel, then join results. Used in parallel sorting, tree traversal, and divide-and-conquer algorithms.

**Map-Reduce**: Apply a function to each element (map), then aggregate results (reduce). Common in big data processing and parallel aggregation.

**Pipeline**: Chain multiple stages where output of one stage feeds into the next. Useful for stream processing and data transformation pipelines.

**Work Stealing**: Each worker has a local queue; idle workers steal tasks from others. Provides better load balancing than centralized queues.

## Pros

- **Higher-level abstraction**: Focus on what to do, not how to schedule
- **Better composability**: Tasks can be combined and chained
- **Improved load balancing**: Runtime can distribute work optimally
- **Reduced boilerplate**: Less manual thread management code
- **Easier reasoning**: Clearer separation of concerns
- **Future-proof**: Can leverage new scheduling strategies without code changes

## Cons

- **Overhead**: Task creation and future management adds overhead
- **Complexity**: Understanding task lifecycles and dependencies can be challenging
- **Debugging**: Harder to debug than sequential code due to non-determinism
- **Exception handling**: Requires careful propagation through futures
- **Memory usage**: Many small tasks can increase memory pressure
- **Limited control**: Less fine-grained control over thread behavior

## Underlying Implementation

### Task Scheduler

A task scheduler manages task execution, handling dependencies and worker coordination:

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

Continuation passing allows chaining tasks where the output of one task feeds into the next:

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

For complex workflows with dependencies, a task graph executor manages execution order:

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

For performance-critical applications, task affinity pins tasks to specific CPU cores:

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

1. **Prefer task-based over thread-based**: Focus on work units rather than thread management
2. **Use appropriate granularity**: Tasks should be large enough to amortize overhead but small enough for parallelism
3. **Implement work stealing**: For load balancing across heterogeneous workloads
4. **Handle dependencies carefully**: Avoid circular dependencies that can cause deadlocks
5. **Use continuations for composition**: Chain tasks naturally for complex workflows
6. **Consider task affinity**: For cache locality in performance-critical code
7. **Monitor queue lengths**: Track task queue sizes for performance tuning
8. **Use exception-safe execution**: Ensure exceptions propagate correctly through futures
9. **Implement cancellation**: Support task cancellation for responsive applications
10. **Profile and tune**: Find optimal task chunk sizes for your specific workload
