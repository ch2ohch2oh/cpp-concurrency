#include <shared_mutex>
#include <thread>
#include <iostream>
#include <vector>

std::shared_mutex shared_mtx;
int shared_data = 0;

void reader(int id) {
    std::shared_lock<std::shared_mutex> lock(shared_mtx);
    std::cout << "Reader " << id << ": Read " << shared_data << "\n";
}

void writer(int id, int value) {
    std::unique_lock<std::shared_mutex> lock(shared_mtx);
    shared_data = value;
    std::cout << "Writer " << id << ": Written " << value << "\n";
}

int main() {
    std::vector<std::thread> threads;
    
    // Create multiple readers
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(reader, i);
    }
    
    // Create a writer
    threads.emplace_back(writer, 0, 42);
    
    // Create more readers
    for (int i = 3; i < 5; ++i) {
        threads.emplace_back(reader, i);
    }
    
    // Create another writer
    threads.emplace_back(writer, 1, 100);
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
