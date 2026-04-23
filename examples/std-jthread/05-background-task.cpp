#include <thread>
#include <string>
#include <fstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>

class AsyncLogger {
public:
    AsyncLogger(const std::string& filename) 
        : filename_(filename), running_(false) {
        
        worker_ = std::jthread([this](std::stop_token stoken) {
            running_ = true;
            std::ofstream file(filename_);
            
            if (!file) {
                std::cerr << "Failed to open log file: " << filename_ << "\n";
                return;
            }
            
            while (true) {
                std::string message;
                
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, stoken, [this, &stoken] {
                        return !queue_.empty() || stoken.stop_requested();
                    });
                    
                    if (queue_.empty() && stoken.stop_requested()) {
                        break;
                    }
                    
                    if (!queue_.empty()) {
                        message = std::move(queue_.front());
                        queue_.pop();
                    }
                }
                
                if (!message.empty()) {
                    file << message << std::endl;
                    file.flush();  // Ensure data is written
                }
            }
            
            // Flush remaining messages before exit
            std::lock_guard<std::mutex> lock(mutex_);
            while (!queue_.empty()) {
                file << queue_.front() << std::endl;
                queue_.pop();
            }
            
            std::cout << "Logger: Shutdown complete\n";
        });
    }
    
    void log(const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << " [" << std::this_thread::get_id() << "] " << message;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(ss.str());
        }
        cv_.notify_one();
    }
    
    ~AsyncLogger() {
        if (worker_.joinable()) {
            worker_.request_stop();
            cv_.notify_all();
            // jthread automatically joins
        }
    }

private:
    std::string filename_;
    std::jthread worker_;
    std::queue<std::string> queue_;
    mutable std::mutex mutex_;
    std::condition_variable_any cv_;
    std::atomic<bool> running_;
};

class MetricsCollector {
public:
    MetricsCollector() : running_(false) {
        worker_ = std::jthread([this](std::stop_token stoken) {
            running_ = true;
            
            while (!stoken.stop_requested()) {
                // Simulate metrics collection
                collect_metrics();
                
                // Wait for 1 second or stop request
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            std::cout << "MetricsCollector: Final report\n";
            print_summary();
        });
    }
    
    void record_event(const std::string& event_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        events_[event_name]++;
    }
    
    ~MetricsCollector() {
        if (worker_.joinable()) {
            worker_.request_stop();
            // jthread automatically joins
        }
    }

private:
    void collect_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        // In real implementation, would collect CPU, memory, etc.
        std::cout << "Metrics: Collected " << total_events() << " total events\n";
    }
    
    void print_summary() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "=== Metrics Summary ===\n";
        for (const auto& [name, count] : events_) {
            std::cout << "  " << name << ": " << count << "\n";
        }
    }
    
    size_t total_events() const {
        size_t total = 0;
        for (const auto& [name, count] : events_) {
            total += count;
        }
        return total;
    }

private:
    std::jthread worker_;
    std::mutex mutex_;
    std::unordered_map<std::string, size_t> events_;
    std::atomic<bool> running_;
};

int main() {
    // Initialize background services
    AsyncLogger logger("app.log");
    MetricsCollector metrics;
    
    std::cout << "Application starting...\n";
    logger.log("Application started");
    
    // Simulate application work
    std::vector<std::jthread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&logger, &metrics, i](std::stop_token stoken) {
            for (int j = 0; j < 5; ++j) {
                if (stoken.stop_requested()) {
                    break;
                }
                
                logger.log("Worker " + std::to_string(i) + " processing item " + std::to_string(j));
                metrics.record_event("worker_" + std::to_string(i) + "_process");
                
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });
    }
    
    // Let workers run
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Shutting down...\n";
    logger.log("Initiating shutdown");
    
    // Request all workers to stop
    for (auto& worker : workers) {
        worker.request_stop();
    }
    
    // Workers automatically join on destruction
    logger.log("Shutdown complete");
    
    std::cout << "Main: Exiting\n";
    
    // Background services automatically shut down via destructors
    return 0;
}
