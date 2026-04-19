#include <iostream>
#include <semaphore>
#include <thread>
#include <queue>
#include <functional>
#include <vector>

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::counting_semaphore<> task_available_{0};
    std::counting_semaphore<> task_slots_{100};  // Limit queue size
    std::mutex mtx_;
    bool stop_;
    
public:
    ThreadPool(size_t threads) : stop_(false) {
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
    bool enqueue(F&& f) {
        if (!task_slots_.try_acquire_for(std::chrono::seconds(1))) {
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
    
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }

        for (size_t i = 0; i < workers_.size(); ++i) {
            task_available_.release();
        }

        for (auto& worker : workers_) {
            worker.join();
        }
    }
};

int main() {
    ThreadPool pool(4);
    
    // Enqueue some tasks
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i]() {
            std::cout << "Task " << i << " executed\n";
        });
    }
    
    // Give tasks time to complete (in real code, use proper synchronization)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    return 0;
}
