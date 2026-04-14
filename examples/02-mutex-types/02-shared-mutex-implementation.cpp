#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>

// Simplified implementation of shared_mutex (reader-writer lock)
// This demonstrates the underlying synchronization logic
class shared_mutex {
private:
    std::mutex mtx_;
    std::condition_variable read_cv_;
    std::condition_variable write_cv_;
    int readers_;
    bool writer_;
    bool write_pending_;
    
public:
    shared_mutex() : readers_(0), writer_(false), write_pending_(false) {}
    
    void lock() {  // Exclusive lock (writer)
        std::unique_lock<std::mutex> lock(mtx_);
        write_pending_ = true;
        write_cv_.wait(lock, [this] { 
            return !writer_ && readers_ == 0; 
        });
        writer_ = true;
        write_pending_ = false;
    }
    
    void unlock() {  // Unlock exclusive
        std::lock_guard<std::mutex> lock(mtx_);
        writer_ = false;
        read_cv_.notify_all();
    }
    
    void lock_shared() {  // Shared lock (reader)
        std::unique_lock<std::mutex> lock(mtx_);
        read_cv_.wait(lock, [this] { 
            return !writer_ && !write_pending_; 
        });
        ++readers_;
    }
    
    void unlock_shared() {  // Unlock shared
        std::lock_guard<std::mutex> lock(mtx_);
        --readers_;
        if (readers_ == 0) {
            write_cv_.notify_one();
        }
    }
};

// Test the implementation
shared_mutex mtx;
int shared_data = 0;

void reader(int id) {
    mtx.lock_shared();
    std::cout << "Reader " << id << ": Read " << shared_data << "\n";
    mtx.unlock_shared();
}

void writer(int id, int value) {
    mtx.lock();
    shared_data = value;
    std::cout << "Writer " << id << ": Written " << value << "\n";
    mtx.unlock();
}

int main() {
    std::thread r1(reader, 1);
    std::thread r2(reader, 2);
    std::thread w1(writer, 1, 42);
    
    r1.join();
    r2.join();
    w1.join();
    
    return 0;
}
