#include <thread>
#include <iostream>
#include <chrono>

void worker_function(int id) {
    for (int i = 0; i < 5; ++i) {
        std::cout << "Worker " << id << " iteration " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    // jthread automatically joins on destruction
    std::jthread t1(worker_function, 1);
    std::jthread t2(worker_function, 2);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // No need to call join() - happens automatically
    return 0;
}
