#include <future>
#include <thread>
#include <iostream>

void worker(std::promise<int> p) {
    try {
        // Do some work
        int result = 42;
        p.set_value(result);
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(worker, std::move(prom));
    
    // Wait for result
    int result = fut.get();
    std::cout << "Result: " << result << "\n";
    
    t.join();
    return 0;
}
