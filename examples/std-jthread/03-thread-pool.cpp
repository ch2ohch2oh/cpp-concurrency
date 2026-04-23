#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <future>
#include <type_traits>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            // Note: std::jthread ALWAYS passes stop_token as first parameter
            workers_.emplace_back([this](std::stop_token stoken) {
                while (true) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        // Wait until stop is requested or there's a task
                        cv_.wait(lock, stoken, [this] {
                            return stop_ || !tasks_.empty();
                        });
                        
                        // Exit if stop was requested and no tasks remain
                        if (stoken.stop_requested() && tasks_.empty()) {
                            return;
                        }
                        
                        if (!tasks_.empty()) {
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                    }
                    
                    if (task) {
                        task();
                    }
                }
            });
        }
    }
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> result = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks_.emplace([task]() { (*task)(); });
        }
        
        cv_.notify_one();
        return result;
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        // Request all threads to stop
        for (auto& worker : workers_) {
            worker.request_stop();
        }
        
        cv_.notify_all();
        
        // jthreads will automatically join
    }
    
    size_t queue_size() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    mutable std::mutex queue_mutex_;
    std::condition_variable_any cv_;
    std::queue<std::function<void()>> tasks_;
    std::atomic<bool> stop_;
    std::vector<std::jthread> workers_;
};

int main() {
    ThreadPool pool(4);
    
    // Enqueue some tasks
    std::vector<std::future<int>> results;
    
    for (int i = 0; i < 8; ++i) {
        results.emplace_back(pool.enqueue([i] {
            std::cout << "Task " << i << " started\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100 * (i % 3 + 1)));
            std::cout << "Task " << i << " completed\n";
            return i * i;
        }));
    }
    
    // Wait for all tasks to complete
    for (auto& result : results) {
        std::cout << "Result: " << result.get() << "\n";
    }
    
    std::cout << "All tasks completed. Queue size: " << pool.queue_size() << "\n";
    
    return 0;
}
