#include <thread>
#include <vector>
#include <iostream>
#include <mutex>
#include <condition_variable>

class AdaptiveThreadPool {
private:
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_;
    size_t active_threads_;
    
public:
    AdaptiveThreadPool() : stop_(false), active_threads_(0) {
        size_t max_threads = std::thread::hardware_concurrency();
        for (size_t i = 0; i < max_threads; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }
    
    void worker_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return stop_; });
            
            if (stop_) return;
            
            // Do work
            active_threads_++;
            lock.unlock();
            
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            lock.lock();
            active_threads_--;
        }
    }
    
    ~AdaptiveThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            worker.join();
        }
    }
    
    size_t get_active_threads() const { return active_threads_; }
};

int main() {
    AdaptiveThreadPool pool;
    std::cout << "Pool created with " << std::thread::hardware_concurrency() << " threads\n";
    return 0;
}
