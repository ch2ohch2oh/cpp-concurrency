#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>

int main() {
    std::vector<int> data(1000000);
    std::mt19937 rng(42);
    std::generate(data.begin(), data.end(), [&]() { return rng(); });
    
    // Sequential sort
    auto start = std::chrono::high_resolution_clock::now();
    std::sort(data.begin(), data.end());
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Sequential: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";
    
    // Parallel sort
    std::generate(data.begin(), data.end(), [&]() { return rng(); });
    start = std::chrono::high_resolution_clock::now();
    std::sort(std::execution::par, data.begin(), data.end());
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Parallel: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";
    
    return 0;
}
