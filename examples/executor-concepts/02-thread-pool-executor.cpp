#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <functional>

class ThreadPoolExecutor {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    
public:
    ThreadPoolExecutor(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    
    template<typename F>
    void execute(F&& f) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.emplace(std::forward<F>(f));
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

int main() {
    ThreadPoolExecutor executor(4);
    
    for (int i = 0; i < 8; ++i) {
        executor.execute([i]() {
            std::cout << "Task " << i << " executed\n";
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
