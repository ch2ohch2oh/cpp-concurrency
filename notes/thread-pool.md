# Thread Pool Implementation: Usage and Implementation

## Practical Usage

### Basic Thread Pool

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ThreadPool(size_t threads) : stop_(false) {
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
    
    ~ThreadPool() {
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

### Thread Pool with Return Values

```cpp
#include <future>
#include <vector>

class ThreadPoolWithResult {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ThreadPoolWithResult(size_t threads) : stop_(false) {
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
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type> {
        
        using ReturnType = typename std::result_of<F(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }
    
    ~ThreadPoolWithResult() {
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

### Priority Thread Pool

```cpp
#include <queue>
#include <functional>

struct Task {
    std::function<void()> func;
    int priority;
    
    bool operator<(const Task& other) const {
        return priority < other.priority;  // Higher priority first
    }
};

class PriorityThreadPool {
private:
    std::vector<std::thread> workers_;
    std::priority_queue<Task> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    PriorityThreadPool(size_t threads) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task = std::move(const_cast<Task&>(tasks_.top()));
                        tasks_.pop();
                    }
                    task.func();
                }
            });
        }
    }
    
    template<class F>
    void enqueue(F&& f, int priority = 0) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.push({std::forward<F>(f), priority});
        }
        cv_.notify_one();
    }
    
    ~PriorityThreadPool() {
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

### Realistic Example: Web Server Thread Pool

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <memory>

class HttpRequest;
class HttpResponse;

class WebServerThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<HttpResponse(HttpRequest)>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    size_t active_workers_;
    std::condition_variable idle_cv_;
    
public:
    WebServerThreadPool(size_t threads) : stop_(false), active_workers_(0) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<HttpResponse(HttpRequest)> task;
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
                    
                    try {
                        HttpRequest request;
                        HttpResponse response = task(request);
                        // Send response
                    } catch (const std::exception& e) {
                        // Handle error
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(mtx_);
                        --active_workers_;
                        idle_cv_.notify_one();
                    }
                }
            });
        }
    }
    
    void handle_request(std::function<HttpResponse(HttpRequest)> handler) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("Server is shutting down");
            }
            tasks_.push(handler);
        }
        cv_.notify_one();
    }
    
    void wait_for_idle() {
        std::unique_lock<std::mutex> lock(mtx_);
        idle_cv_.wait(lock, [this] { 
            return tasks_.empty() && active_workers_ == 0; 
        });
    }
    
    ~WebServerThreadPool() {
        wait_for_idle();
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

## Underlying Implementation

### Thread Pool Architecture

```
Main Thread              Worker Threads
    |                        |
    v                        v
[Task Queue] <---[Mutex]---+
    |                        |
    +---->[Condition Variable]
```

### Work Stealing Thread Pool

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

### Dynamic Thread Pool

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

1. Choose appropriate thread pool size (typically number of CPU cores)
2. Use work stealing for load balancing
3. Implement bounded queues to prevent memory exhaustion
4. Handle exceptions properly in worker threads
5. Provide graceful shutdown mechanism
6. Consider priority for different task types
7. Monitor thread pool metrics (queue size, active workers)
8. Use thread-local storage for per-thread resources
9. Avoid task dependencies when possible
10. Profile to optimize chunk sizes and worker count
