#include <future>
#include <thread>
#include <iostream>

int main() {
    // Create a promise and get its associated future
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    // Convert the unique future to a shared_future
    // shared_future is copyable, allowing multiple threads to access the same result
    // std::future is move-only and can only call get() once
    std::shared_future<int> shared = fut.share();
    
    // Launch two threads that both capture the shared_future by value
    // Since shared_future is copyable, each thread gets its own copy
    // Both threads can call get() to read the same result
    std::thread t1([shared] {
        std::cout << "Thread 1: " << shared.get() << "\n";
    });
    
    std::thread t2([shared] {
        std::cout << "Thread 2: " << shared.get() << "\n";
    });
    
    // Set the value in the promise
    // This will unblock both threads waiting on the shared_future
    prom.set_value(42);
    
    // Wait for both threads to finish
    t1.join();
    t2.join();
    
    return 0;
}
