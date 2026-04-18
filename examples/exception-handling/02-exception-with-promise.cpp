#include <thread>
#include <future>
#include <iostream>

void worker(std::promise<int> prom) {
    try {
        throw std::runtime_error("Worker error");
        prom.set_value(42);
    } catch (...) {
        prom.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();
    
    std::thread t(worker, std::move(prom));
    
    try {
        int result = fut.get();
        std::cout << "Result: " << result << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Caught exception from thread: " << e.what() << "\n";
    }
    
    t.join();
    return 0;
}
