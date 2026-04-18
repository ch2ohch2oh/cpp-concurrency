# Executor Concepts: Usage and Implementation

## Practical Usage

### Basic Executor Interface

```cpp
#include <functional>
#include <memory>
#include <future>

class Executor {
public:
    virtual ~Executor() = default;
    
    virtual void execute(std::function<void()> task) = 0;
    
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));
        
        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto future = promise->get_future();
        
        execute([promise, f = std::forward<F>(f), args...]() mutable {
            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    f(args...);
                    promise->set_value();
                } else {
                    promise->set_value(f(args...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        
        return future;
    }
};
```

### Thread Pool Executor

```cpp
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

class ThreadPoolExecutor : public Executor {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ThreadPoolExecutor(size_t threads) : stop_(false) {
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
    
    void execute(std::function<void()> task) override {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("Executor is stopped");
            }
            tasks_.emplace(std::move(task));
        }
        cv_.notify_one();
    }
    
    ~ThreadPoolExecutor() {
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

### Inline Executor (Current Thread)

```cpp
class InlineExecutor : public Executor {
public:
    void execute(std::function<void()> task) override {
        task();
    }
};
```

### Serial Executor (Single Thread)

```cpp
class SerialExecutor : public Executor {
private:
    std::thread thread_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    SerialExecutor() : stop_(false) {
        thread_ = std::thread([this] {
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
    
    void execute(std::function<void()> task) override {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("Executor is stopped");
            }
            tasks_.emplace(std::move(task));
        }
        cv_.notify_one();
    }
    
    ~SerialExecutor() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        thread_.join();
    }
};
```

### Priority Executor

```cpp
#include <queue>

struct PriorityTask {
    std::function<void()> task;
    int priority;
    
    bool operator<(const PriorityTask& other) const {
        return priority < other.priority;  // Higher priority first
    }
};

class PriorityExecutor : public Executor {
private:
    std::vector<std::thread> workers_;
    std::priority_queue<PriorityTask> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    PriorityExecutor(size_t threads, int priority = 0) : stop_(false) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    PriorityTask task_wrapper;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { 
                            return stop_ || !tasks_.empty(); 
                        });
                        
                        if (stop_ && tasks_.empty()) return;
                        
                        task_wrapper = std::move(const_cast<PriorityTask&>(tasks_.top()));
                        tasks_.pop();
                    }
                    task_wrapper.task();
                }
            });
        }
    }
    
    void execute(std::function<void()> task, int priority = 0) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stop_) {
                throw std::runtime_error("Executor is stopped");
            }
            tasks_.push({std::move(task), priority});
        }
        cv_.notify_one();
    }
    
    void execute(std::function<void()> task) override {
        execute(task, 0);
    }
    
    ~PriorityExecutor() {
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

### Realistic Example: Async HTTP Client with Executor

```cpp
#include <future>
#include <functional>

class HttpClient {
private:
    Executor& executor_;
    
public:
    HttpClient(Executor& executor) : executor_(executor) {}
    
    std::future<std::string> get(const std::string& url) {
        return executor_.submit([url]() {
            // Simulate HTTP request
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return "Response from " + url;
        });
    }
    
    std::future<std::string> post(const std::string& url, const std::string& data) {
        return executor_.submit([url, data]() {
            // Simulate HTTP POST
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            return "POST to " + url + " with " + data;
        });
    }
};

int main() {
    ThreadPoolExecutor executor(4);
    HttpClient client(executor);
    
    auto future1 = client.get("https://example.com");
    auto future2 = client.post("https://api.example.com", "data");
    
    std::cout << future1.get() << "\n";
    std::cout << future2.get() << "\n";
    
    return 0;
}
```

## Underlying Implementation

### Executor Concept (C++ Standard Proposal)

```cpp
// C++ executor concepts (proposed for standardization)
namespace std::execution {
    
    // Executor concept
    template<class E>
    concept executor =
        requires(E e, std::function<void()> f) {
            { e.execute(f) } -> std::same_as<void>;
        };
    
    // One-way executor
    template<class E>
    concept one_way_executor = executor<E>;
    
    // Two-way executor (returns future)
    template<class E>
    concept two_way_executor = executor<E> &&
        requires(E e, std::function<int()> f) {
            { e.twoway_execute(f) } -> std::convertible_to<std::future<int>>;
        };
    
    // Bulk executor
    template<class E>
    concept bulk_executor = executor<E> &&
        requires(E e, std::function<void(size_t)> f, size_t n) {
            { e.bulk_execute(f, n) } -> std::same_as<void>;
        };
}
```

### Executor Traits

```cpp
template<class Executor>
class executor_traits {
public:
    using executor_type = Executor;
    
    template<class F, class... Args>
    static auto execute(Executor& ex, F&& f, Args&&... args)
        -> std::future<decltype(f(args...))>;
    
    static constexpr bool is_twoway = /* detection */;
    static constexpr bool is_bulk = /* detection */;
};
```

### Then-Based Executor

```cpp
template<class Executor>
class ThenExecutor {
private:
    Executor& base_;
    
public:
    ThenExecutor(Executor& base) : base_(base) {}
    
    template<class F, class... Args>
    auto then(F&& f, Args&&... args) {
        return base_.submit(std::forward<F>(f), std::forward<Args>(args)...);
    }
    
    template<class F, class... Args>
    auto then(std::future<decltype(f(args...))>& fut, F&& f, Args&&... args) {
        using ReturnType = decltype(f(args...));
        auto promise = std::make_shared<std::promise<ReturnType>>();
        auto result = promise->get_future();
        
        base_.execute([fut = std::move(fut), promise, f = std::forward<F>(f), args...]() mutable {
            try {
                auto prev_result = fut.get();
                if constexpr (std::is_void_v<ReturnType>) {
                    f(prev_result, args...);
                    promise->set_value();
                } else {
                    promise->set_value(f(prev_result, args...));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });
        
        return result;
    }
};
```

### WhenAll Executor

```cpp
template<class Executor>
class WhenAllExecutor {
private:
    Executor& base_;
    
public:
    WhenAllExecutor(Executor& base) : base_(base) {}
    
    template<class... Futures>
    auto when_all(Futures&&... futures) {
        using TupleType = std::tuple<typename Futures::value_type...>;
        
        auto promise = std::make_shared<std::promise<TupleType>>();
        auto result = promise->get_future();
        
        auto counter = std::make_shared<std::atomic<size_t>>(sizeof...(Futures));
        auto tuple = std::make_shared<TupleType>();
        
        size_t index = 0;
        (base_.execute([fut = std::forward<Futures>(futures), promise, tuple, counter, index]() mutable {
            try {
                std::get<index>(*tuple) = fut.get();
                if (--*counter == 0) {
                    promise->set_value(*tuple);
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
            ++index;
        }), ...);
        
        return result;
    }
};
```

### Executor with Work Stealing

```cpp
class WorkStealingExecutor : public Executor {
private:
    struct Worker {
        std::deque<std::function<void()>> local_queue;
        std::mutex mtx;
        std::thread thread;
        size_t id;
    };
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> next_worker_;
    
public:
    WorkStealingExecutor(size_t num_workers) : next_worker_(0) {
        for (size_t i = 0; i < num_workers; ++i) {
            workers_.push_back(std::make_unique<Worker>());
            workers_[i]->id = i;
            workers_[i]->thread = std::thread([this, i] { worker_loop(i); });
        }
    }
    
    void worker_loop(size_t worker_id) {
        while (true) {
            std::function<void()> task = get_task(worker_id);
            if (task) {
                task();
            } else {
                std::this_thread::yield();
            }
        }
    }
    
    std::function<void()> get_task(size_t worker_id) {
        // Try local queue
        {
            std::lock_guard<std::mutex> lock(workers_[worker_id]->mtx);
            if (!workers_[worker_id]->local_queue.empty()) {
                auto task = std::move(workers_[worker_id]->local_queue.front());
                workers_[worker_id]->local_queue.pop_front();
                return task;
            }
        }
        
        // Try to steal
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
    
    void execute(std::function<void()> task) override {
        size_t worker_id = next_worker_++ % workers_.size();
        
        std::lock_guard<std::mutex> lock(workers_[worker_id]->mtx);
        workers_[worker_id]->local_queue.push_back(std::move(task));
    }
};
```

### Executor with Context

```cpp
class ExecutorContext {
private:
    Executor& executor_;
    std::unordered_map<std::string, std::any> context_;
    std::mutex mtx_;
    
public:
    ExecutorContext(Executor& executor) : executor_(executor) {}
    
    void set(const std::string& key, std::any value) {
        std::lock_guard<std::mutex> lock(mtx_);
        context_[key] = std::move(value);
    }
    
    template<typename T>
    T get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        return std::any_cast<T>(context_[key]);
    }
    
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) {
        return executor_.submit([this, f = std::forward<F>(f), args...]() mutable {
            // Task has access to context
            return f(args...);
        });
    }
};
```

### Executor Scheduler

```cpp
class ScheduledExecutor : public Executor {
private:
    struct ScheduledTask {
        std::function<void()> task;
        std::chrono::steady_clock::time_point deadline;
        
        bool operator<(const ScheduledTask& other) const {
            return deadline > other.deadline;  // Earlier first
        }
    };
    
    std::thread scheduler_;
    std::priority_queue<ScheduledTask> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    Executor& base_;
    
public:
    ScheduledExecutor(Executor& base) : base_(base), stop_(false) {
        scheduler_ = std::thread([this] {
            while (true) {
                ScheduledTask task_wrapper;
                {
                    std::unique_lock<std::mutex> lock(mtx_);
                    
                    if (tasks_.empty()) {
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    } else {
                        auto now = std::chrono::steady_clock::now();
                        if (tasks_.top().deadline > now) {
                            cv_.wait_until(lock, tasks_.top().deadline);
                        }
                    }
                    
                    if (stop_ && tasks_.empty()) return;
                    
                    auto now = std::chrono::steady_clock::now();
                    while (!tasks_.empty() && tasks_.top().deadline <= now) {
                        task_wrapper = std::move(const_cast<ScheduledTask&>(tasks_.top()));
                        tasks_.pop();
                    }
                }
                
                if (task_wrapper.task) {
                    base_.execute(std::move(task_wrapper.task));
                }
            }
        });
    }
    
    void schedule_at(std::chrono::steady_clock::time_point deadline, 
                    std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.push({std::move(task), deadline});
        }
        cv_.notify_one();
    }
    
    void schedule_after(std::chrono::milliseconds delay, std::function<void()> task) {
        schedule_at(std::chrono::steady_clock::now() + delay, std::move(task));
    }
    
    void execute(std::function<void()> task) override {
        base_.execute(std::move(task));
    }
    
    ~ScheduledExecutor() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        scheduler_.join();
    }
};
```

## Best Practices

1. Choose executor type based on concurrency requirements
2. Use inline executor for simple, non-blocking operations
3. Use thread pool for CPU-bound parallel work
4. Use serial executor for tasks requiring ordering
5. Consider priority executor for QoS requirements
6. Use work stealing for load balancing
7. Implement proper exception handling in executors
8. Provide graceful shutdown mechanisms
9. Monitor executor metrics (queue size, active workers)
10. Consider executor composition for complex workflows
