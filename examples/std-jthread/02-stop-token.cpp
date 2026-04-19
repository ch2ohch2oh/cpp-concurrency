#include <thread>
#include <iostream>
#include <chrono>

void interruptible_worker(std::stop_token stoken, int id) {
    int count = 0;
    while (!stoken.stop_requested()) {
        std::cout << "Worker " << id << " count: " << count++ << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Worker " << id << " stopping\n";
}

int main() {
    std::jthread worker(interruptible_worker, 1);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Request stop - thread will exit gracefully
    worker.request_stop();
    
    return 0;
}
