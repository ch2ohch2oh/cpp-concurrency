#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <iostream>

class SafeThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    SafeThreadPool(size_t threads) : stop_(false) {
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
                    
                    try {
                        task();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in worker: " << e.what() << "\n";
                    } catch (...) {
                        std::cerr << "Unknown exception in worker\n";
                    }
                }
            });
        }
    }
    
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<decltype(f(args...))> {
        
        using ReturnType = decltype(f(args...));
        
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
    
    ~SafeThreadPool() {
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

int main() {
    SafeThreadPool pool(4);
    
    // Enqueue some tasks that throw exceptions
    auto fut1 = pool.enqueue([]() {
        std::cout << "Task 1 running\n";
        throw std::runtime_error("Task 1 failed");
    });
    
    auto fut2 = pool.enqueue([]() {
        std::cout << "Task 2 running\n";
        std::cout << "Task 2 completed successfully\n";
    });
    
    auto fut3 = pool.enqueue([]() {
        std::cout << "Task 3 running\n";
        throw std::logic_error("Task 3 logic error");
    });
    
    // Wait for tasks
    try {
        fut1.get();
    } catch (const std::exception& e) {
        std::cout << "Caught from task1: " << e.what() << "\n";
    }
    
    try {
        fut2.get();
    } catch (const std::exception& e) {
        std::cout << "Caught from task2: " << e.what() << "\n";
    }
    
    try {
        fut3.get();
    } catch (const std::exception& e) {
        std::cout << "Caught from task3: " << e.what() << "\n";
    }
    
    return 0;
}
