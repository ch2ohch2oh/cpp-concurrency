#include <latch>
#include <thread>
#include <vector>
#include <iostream>

void worker(std::latch& done, int id) {
    std::cout << "Worker " << id << " starting\n";
    // Do work
    done.count_down();  // Signal completion
    std::cout << "Worker " << id << " done\n";
}

int main() {
    const int num_workers = 4;
    std::latch done(num_workers);
    
    std::vector<std::thread> workers;
    for (int i = 0; i < num_workers; ++i) {
        workers.emplace_back(worker, std::ref(done), i);
    }
    
    // Wait for all workers to finish
    done.wait();
    std::cout << "All workers completed\n";
    
    for (auto& t : workers) {
        t.join();
    }
    
    return 0;
}
