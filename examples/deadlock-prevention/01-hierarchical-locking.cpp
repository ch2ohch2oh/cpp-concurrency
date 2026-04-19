#include <mutex>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <chrono>

class LockHierarchy {
public:
    static constexpr int LEVEL_DATABASE = 1;
    static constexpr int LEVEL_TABLE = 2;
    static constexpr int LEVEL_ROW = 3;
    
    static void lock(std::mutex& mtx, int level) {
        if (!lock_stack_.empty() && lock_stack_.back() >= level) {
            throw std::runtime_error("Lock hierarchy violation: trying to acquire level " + 
                                     std::to_string(level) + " while holding level " + 
                                     std::to_string(lock_stack_.back()));
        }
        mtx.lock();
        lock_stack_.push_back(level);
    }
    
    static void unlock(std::mutex& mtx, int level) {
        if (lock_stack_.empty() || lock_stack_.back() != level) {
            throw std::runtime_error("Unlock hierarchy violation: trying to unlock level " + 
                                     std::to_string(level) + " but top of stack is " + 
                                     (lock_stack_.empty() ? "empty" : std::to_string(lock_stack_.back())));
        }
        mtx.unlock();
        lock_stack_.pop_back();
    }
    
    static int current_level() { 
        return lock_stack_.empty() ? 0 : lock_stack_.back(); 
    }
    
private:
    static thread_local std::vector<int> lock_stack_;
};

thread_local std::vector<int> LockHierarchy::lock_stack_;

// Simulated database resources
std::mutex database_mtx;
std::mutex table_mtx;
std::mutex row_mtx;

void correct_order_thread() {
    std::cout << "Thread 1: Acquiring locks in correct order (DB -> Table -> Row)\n";
    
    LockHierarchy::lock(database_mtx, LockHierarchy::LEVEL_DATABASE);
    std::cout << "Thread 1: Locked database (level 1)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LockHierarchy::lock(table_mtx, LockHierarchy::LEVEL_TABLE);
    std::cout << "Thread 1: Locked table (level 2)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LockHierarchy::lock(row_mtx, LockHierarchy::LEVEL_ROW);
    std::cout << "Thread 1: Locked row (level 3)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Unlock in reverse order
    LockHierarchy::unlock(row_mtx, LockHierarchy::LEVEL_ROW);
    std::cout << "Thread 1: Unlocked row\n";
    
    LockHierarchy::unlock(table_mtx, LockHierarchy::LEVEL_TABLE);
    std::cout << "Thread 1: Unlocked table\n";
    
    LockHierarchy::unlock(database_mtx, LockHierarchy::LEVEL_DATABASE);
    std::cout << "Thread 1: Unlocked database\n";
}

void incorrect_order_thread() {
    std::cout << "Thread 2: Attempting to acquire locks in incorrect order (Row -> Table)\n";
    
    try {
        LockHierarchy::lock(row_mtx, LockHierarchy::LEVEL_ROW);
        std::cout << "Thread 2: Locked row (level 3)\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // This should throw - trying to acquire level 2 while holding level 3
        LockHierarchy::lock(table_mtx, LockHierarchy::LEVEL_TABLE);
        std::cout << "Thread 2: Locked table (level 2) - THIS SHOULD NOT HAPPEN\n";
        
        LockHierarchy::unlock(table_mtx, LockHierarchy::LEVEL_TABLE);
        LockHierarchy::unlock(row_mtx, LockHierarchy::LEVEL_ROW);
    } catch (const std::runtime_error& e) {
        std::cout << "Thread 2: Caught exception: " << e.what() << "\n";
        LockHierarchy::unlock(row_mtx, LockHierarchy::LEVEL_ROW);
    }
}

void partial_order_thread() {
    std::cout << "Thread 3: Acquiring only database and table locks\n";
    
    LockHierarchy::lock(database_mtx, LockHierarchy::LEVEL_DATABASE);
    std::cout << "Thread 3: Locked database (level 1)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LockHierarchy::lock(table_mtx, LockHierarchy::LEVEL_TABLE);
    std::cout << "Thread 3: Locked table (level 2)\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    LockHierarchy::unlock(table_mtx, LockHierarchy::LEVEL_TABLE);
    std::cout << "Thread 3: Unlocked table\n";
    
    LockHierarchy::unlock(database_mtx, LockHierarchy::LEVEL_DATABASE);
    std::cout << "Thread 3: Unlocked database\n";
}

int main() {
    std::cout << "=== Hierarchical Locking Demo ===\n\n";
    
    // Thread 1: Correct order
    std::thread t1(correct_order_thread);
    t1.join();
    
    std::cout << "\n---\n\n";
    
    // Thread 2: Incorrect order (should throw)
    std::thread t2(incorrect_order_thread);
    t2.join();
    
    std::cout << "\n---\n\n";
    
    // Thread 3: Partial order (valid)
    std::thread t3(partial_order_thread);
    t3.join();
    
    std::cout << "\n=== Demo Complete ===\n";
    std::cout << "Hierarchical locking prevents deadlock by enforcing lock order at runtime.\n";
    
    return 0;
}
