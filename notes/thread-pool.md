# Thread Pool Implementation

## Motivation

Creating a new thread for each task is expensive due to thread creation overhead and resource consumption. Thread pools solve this by reusing a fixed number of worker threads, significantly improving performance for applications with many short-lived tasks.

Thread pools provide several key benefits:
- **Reduced overhead**: Threads are created once and reused
- **Resource management**: Limits the number of concurrent threads
- **Improved responsiveness**: Tasks can be queued and processed as threads become available
- **Better resource utilization**: Prevents thread exhaustion and context switching overhead

Thread pools are essential for:
- Web servers handling many concurrent requests
- Database connection pools
- Image processing pipelines
- Any application with many independent, short-lived tasks

## Practical Usage

See the `examples/thread-pool/` directory for complete working examples:
- `01-basic-threadpool.cpp` - Simple thread pool with task enqueueing
- `02-threadpool-with-result.cpp` - Thread pool returning futures

### Basic Thread Pool

A basic thread pool consists of worker threads that pull tasks from a shared queue:

- Workers wait on a condition variable for tasks
- Tasks are enqueued and workers are notified
- The pool shuts down gracefully when destroyed

### Thread Pool with Return Values

Using `std::packaged_task` and `std::future`, thread pools can return results:

- Tasks are wrapped in `std::packaged_task`
- Futures are returned to callers for result retrieval
- Exceptions propagate through the future mechanism

### Priority Thread Pool

For quality of service (QoS) requirements, tasks can be prioritized:

- Higher priority tasks are executed first
- Uses `std::priority_queue` instead of regular queue
- Useful for real-time systems or differentiated service levels

## Pros

- **Performance**: Eliminates thread creation overhead for each task
- **Resource control**: Limits maximum concurrent threads
- **Scalability**: Handles bursty workloads efficiently
- **Simplicity**: Abstracts away thread management complexity
- **Flexibility**: Can be extended with priorities, timeouts, etc.
- **Graceful shutdown**: Can wait for queued tasks to complete

## Cons

- **Complexity**: Requires careful synchronization and error handling
- **Deadlock risk**: Poorly designed tasks can deadlock the pool
- **Memory usage**: Queued tasks consume memory
- **Load balancing**: Simple pools may have uneven distribution
- **Exception handling**: Requires special handling for task exceptions
- **Overhead**: Still has synchronization overhead for task enqueueing

## Underlying Implementation

### Thread Pool Architecture

The basic thread pool architecture consists of:

```
Main Thread              Worker Threads
    |                        |
    v                        v
[Task Queue] <---[Mutex]---+
    |                        |
    +---->[Condition Variable]
```

The main thread enqueues tasks, which are then picked up by worker threads. A condition variable is used to efficiently wait for new tasks without busy-waiting.

### Work Stealing Thread Pool

Simple thread pools can suffer from load imbalance if some workers get more work than others. Work stealing addresses this by allowing idle workers to "steal" tasks from other workers' queues:

```cpp
class WorkStealingThreadPool {
private:
    struct Worker {
        std::deque<std::function<void()>> local_queue;
        std::mutex mtx;
        std::thread thread;
        size_t id;
    };
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> next_worker_;
    std::atomic<bool> stop_;
    
public:
    WorkStealingThreadPool(size_t num_threads) : next_worker_(0), stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.push_back(std::make_unique<Worker>());
            workers_[i]->id = i;
            workers_[i]->thread = std::thread([this, i] { worker_loop(i); });
        }
    }
    
    void worker_loop(size_t worker_id) {
        while (!stop_) {
            std::function<void()> task = get_task(worker_id);
            if (task) {
                task();
            } else {
                std::this_thread::yield();
            }
        }
    }
    
    std::function<void()> get_task(size_t worker_id) {
        // Try to get from local queue
        {
            std::lock_guard<std::mutex> lock(workers_[worker_id]->mtx);
            if (!workers_[worker_id]->local_queue.empty()) {
                auto task = std::move(workers_[worker_id]->local_queue.front());
                workers_[worker_id]->local_queue.pop_front();
                return task;
            }
        }
        
        // Try to steal from other workers
        for (size_t i = 1; i < workers_.size(); ++i) {
            size_t victim = (worker_id + i) % workers_.size();
            
            std::lock_guard<std::mutex> lock(workers_[victim]->mtx);
            if (!workers_[victim]->local_queue.empty()) {
                auto task = std::move(workers_[victim]->local_queue.back());
                workers_[victim]->local_queue.pop_back();
                return task;
            }
        }
        
        return nullptr;
    }
    
    void enqueue(std::function<void()> task) {
        size_t worker_id = next_worker_.fetch_add(1) % workers_.size();
        
        std::lock_guard<std::mutex> lock(workers_[worker_id]->mtx);
        workers_[worker_id]->local_queue.push_back(std::move(task));
    }
    
    ~WorkStealingThreadPool() {
        stop_ = true;
        for (auto& worker : workers_) {
            worker->thread.join();
        }
    }
};
```

### Work Stealing vs Single Work Queue

**Single Work Queue:**
- One global queue shared by all threads
- All threads push/pop from the same queue
- Simple to implement
- **Disadvantages:**
  - High contention on the queue (mutex contention)
  - Poor cache locality (threads may work on unrelated tasks)
  - Scalability bottleneck with many threads

**Work Stealing:**
- Each thread has its own local queue (deque)
- Threads work on their own tasks first
- Idle threads steal from others
- **Advantages:**
  - Low contention (threads mostly access their own queue)
  - Better cache locality (threads work on related tasks)
  - Better scalability with many cores
  - Automatic load balancing
- **Disadvantages:**
  - More complex to implement
  - Slightly higher memory overhead (multiple queues)

**When to use each:**

**Single Queue:**
- Small number of threads (1-4)
- Simple workloads
- When implementation simplicity is priority

**Work Stealing:**
- Many threads (8+)
- Complex, irregular workloads
- When performance and scalability matter
- Recursive parallel algorithms (like divide-and-conquer)

### Dynamic Thread Pool

Dynamic thread pools adjust their size based on workload:

```cpp
class DynamicThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_workers_;
    size_t min_threads_;
    size_t max_threads_;
    std::thread manager_thread_;
    
    void manager_loop() {
        while (!stop_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::lock_guard<std::mutex> lock(mtx_);
            
            // Add workers if needed
            if (tasks_.size() > workers_.size() && 
                workers_.size() < max_threads_) {
                add_worker();
            }
            
            // Remove idle workers
            if (active_workers_ < workers_.size() / 2 && 
                workers_.size() > min_threads_) {
                // Signal workers to exit
            }
        }
    }
    
    void add_worker() {
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
                    ++active_workers_;
                }
                
                task();
                
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    --active_workers_;
                }
            }
        });
    }
    
public:
    DynamicThreadPool(size_t min_threads, size_t max_threads)
        : stop_(false), active_workers_(0),
          min_threads_(min_threads), max_threads_(max_threads) {
        
        for (size_t i = 0; i < min_threads; ++i) {
            add_worker();
        }
        
        manager_thread_ = std::thread([this] { manager_loop(); });
    }
    
    ~DynamicThreadPool() {
        stop_ = true;
        cv_.notify_all();
        manager_thread_.join();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Thread Pool with Bounded Queue

To prevent memory exhaustion, thread pools can use bounded queues with semaphores:

```cpp
#include <semaphore>

class BoundedThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::counting_semaphore<> task_slots_;
    std::counting_semaphore<> task_available_;
    bool stop_;
    
public:
    BoundedThreadPool(size_t threads, size_t queue_size)
        : task_slots_(queue_size), task_available_(0), stop_(false) {
        
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    task_available_.acquire();
                    
                    std::function<void()> task;
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task_slots_.release();
                    task();
                }
            });
        }
    }
    
    template<class F>
    bool try_enqueue(F&& f) {
        if (!task_slots_.try_acquire_for(std::chrono::milliseconds(100))) {
            return false;  // Queue full
        }
        
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) return false;
            tasks_.emplace(std::forward<F>(f));
        }
        
        task_available_.release();
        return true;
    }
    
    ~BoundedThreadPool() {
        stop_ = true;
        for (size_t i = 0; i < workers_.size(); ++i) {
            task_available_.release();
        }
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Thread Pool with Task Dependencies

For complex workflows, thread pools can manage task dependencies:

```cpp
#include <unordered_map>
#include <set>

class TaskGraphThreadPool {
private:
    struct Task {
        std::function<void()> func;
        std::set<size_t> dependencies;
        std::set<size_t> dependents;
        bool completed = false;
    };
    
    std::vector<std::thread> workers_;
    std::queue<size_t> ready_tasks_;
    std::unordered_map<size_t, Task> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    std::atomic<size_t> next_task_id_;
    
public:
    TaskGraphThreadPool(size_t threads) : stop_(false), next_task_id_(0) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    size_t task_id;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !ready_tasks_.empty(); 
                        });
                        
                        if (stop_ && ready_tasks_.empty()) return;
                        
                        task_id = ready_tasks_.front();
                        ready_tasks_.pop();
                    }
                    
                    // Execute task
                    tasks_[task_id].func();
                    
                    // Mark as completed
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        tasks_[task_id].completed = true;
                        
                        // Check dependents
                        for (size_t dep_id : tasks_[task_id].dependents) {
                            bool all_deps_complete = true;
                            for (size_t dep : tasks_[dep_id].dependencies) {
                                if (!tasks_[dep].completed) {
                                    all_deps_complete = false;
                                    break;
                                }
                            }
                            if (all_deps_complete) {
                                ready_tasks_.push(dep_id);
                            }
                        }
                    }
                    cv_.notify_all();
                }
            });
        }
    }
    
    size_t add_task(std::function<void()> func, 
                    const std::set<size_t>& dependencies = {}) {
        size_t task_id = next_task_id_++;
        
        std::lock_guard<std::mutex> lock(mtx_);
        tasks_[task_id] = {func, dependencies, {}, false};
        
        // Register dependents
        for (size_t dep_id : dependencies) {
            tasks_[dep_id].dependents.insert(task_id);
        }
        
        // Check if ready
        if (dependencies.empty()) {
            ready_tasks_.push(task_id);
            cv_.notify_one();
        }
        
        return task_id;
    }
    
    ~TaskGraphThreadPool() {
        stop_ = true;
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
};
```

### Thread Pool Performance Considerations

Performance optimizations include cache line padding to prevent false sharing:

```cpp
// Cache line padding to prevent false sharing
struct alignas(64) PaddedCounter {
    std::atomic<uint64_t> value;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
};

class OptimizedThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    PaddedCounter task_count_;
    PaddedCounter completed_count_;
    
public:
    // ... implementation with padded counters
};
```

## Best Practices

1. **Choose appropriate size**: Typically match CPU core count, but adjust based on I/O vs CPU-bound work
2. **Use work stealing**: For load balancing across heterogeneous workloads
3. **Implement bounded queues**: Prevent memory exhaustion from unbounded task queues
4. **Handle exceptions properly**: Catch exceptions in worker threads to prevent pool termination
5. **Provide graceful shutdown**: Wait for queued tasks to complete before destruction
6. **Consider priorities**: For QoS requirements in production systems
7. **Monitor metrics**: Track queue size, active workers, and task completion times
8. **Use thread-local storage**: For per-thread resources like database connections
9. **Avoid complex dependencies**: Prefer independent tasks for simpler pools
10. **Profile and tune**: Optimize chunk sizes, worker counts, and queue policies for your workload
