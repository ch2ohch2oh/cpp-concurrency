#include <semaphore>
#include <thread>
#include <vector>
#include <iostream>

std::counting_semaphore<3> sem(3);  // Max 3 concurrent accesses

void access_resource(int id) {
    sem.acquire();  // Wait for available slot
    std::cout << "Thread " << id << " accessing resource\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Thread " << id << " releasing resource\n";
    sem.release();  // Release slot
}

int main() {
    const int num_threads = 6;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(access_resource, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
