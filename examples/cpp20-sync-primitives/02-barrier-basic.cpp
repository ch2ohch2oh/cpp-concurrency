#include <barrier>
#include <thread>
#include <vector>
#include <iostream>

void phase_worker(std::barrier<>* sync, int id) {
    for (int phase = 0; phase < 3; ++phase) {
        std::cout << "Thread " << id << " phase " << phase << "\n";
        sync->arrive_and_wait();  // Wait for all threads
        std::cout << "Thread " << id << " past barrier\n";
    }
}

int main() {
    const int num_threads = 3;
    std::barrier sync(num_threads);  // Use default completion function
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(phase_worker, &sync, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
