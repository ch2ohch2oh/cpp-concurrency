#include <future>
#include <iostream>

int square(int x) {
    return x * x;
}

int main() {
    // packaged_task wraps a callable and provides a future for its result
    // It's useful when you want to pass a task to a thread but still get the result
    std::packaged_task<int(int)> task(square);
    
    // Get the future before moving the task (you can only call get_future() once)
    std::future<int> fut = task.get_future();
    
    // packaged_task is move-only, so we must std::move it into the thread
    // The thread will execute the task with the provided argument (10)
    std::thread t(std::move(task), 10);
    
    // Wait for the result from the future (blocks until task completes)
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    // Join the thread to wait for it to finish
    t.join();
    return 0;
}
