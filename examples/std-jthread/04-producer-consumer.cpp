#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <chrono>

template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cv_.notify_one();
    }
    
    bool pop(T& value, std::stop_token stoken) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait with stop token support
        cv_.wait(lock, stoken, [this, &stoken] {
            return !queue_.empty() || stoken.stop_requested();
        });
        
        if (queue_.empty()) {
            return false;  // Stop was requested
        }
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::queue<T> queue_;
};

class WorkItem {
public:
    WorkItem(int id, std::string data) : id_(id), data_(std::move(data)) {}
    
    void process() const {
        std::cout << "Processing item " << id_ << ": " << data_ << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    int id() const { return id_; }

private:
    int id_;
    std::string data_;
};

int main() {
    ThreadSafeQueue<WorkItem> queue;
    std::atomic<int> processed_count{0};
    
    // Producer thread
    std::jthread producer([&queue](std::stop_token stoken) {
        for (int i = 0; i < 20; ++i) {
            if (stoken.stop_requested()) {
                std::cout << "Producer: Stop requested, exiting\n";
                break;
            }
            
            queue.push(WorkItem(i, "Data payload " + std::to_string(i)));
            std::cout << "Producer: Pushed item " << i << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        std::cout << "Producer: Finished producing items\n";
    });
    
    // Consumer threads
    std::vector<std::jthread> consumers;
    for (int i = 0; i < 3; ++i) {
        consumers.emplace_back([&queue, &processed_count, i](std::stop_token stoken) {
            int local_count = 0;
            while (true) {
                WorkItem item(0, "");
                if (!queue.pop(item, stoken)) {
                    std::cout << "Consumer " << i << ": Stop requested or queue empty\n";
                    break;
                }
                
                item.process();
                local_count++;
                processed_count++;
            }
            std::cout << "Consumer " << i << ": Processed " << local_count << " items\n";
        });
    }
    
    // Let it run for a bit
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Main: Requesting shutdown...\n";
    
    // Request all threads to stop
    producer.request_stop();
    for (auto& consumer : consumers) {
        consumer.request_stop();
    }
    
    // jthreads automatically join on destruction
    std::cout << "Main: Total items processed: " << processed_count << "\n";
    std::cout << "Main: Queue size: " << queue.size() << "\n";
    
    return 0;
}
