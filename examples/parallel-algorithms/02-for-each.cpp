#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>
#include <numeric>

void process_item(int& item) {
    item *= 2;
}

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 0);
    
    // Sequential for_each
    auto start = std::chrono::high_resolution_clock::now();
    std::for_each(data.begin(), data.end(), process_item);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Sequential: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";

#if __cpp_lib_parallel_algorithms >= 201603
    // Parallel for_each
    start = std::chrono::high_resolution_clock::now();
    std::for_each(std::execution::par, data.begin(), data.end(), process_item);
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Parallel: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms\n";
#else
    std::cout << "Parallel algorithms not available (requires C++17 with library support)\n";
#endif
    
    std::cout << "First element: " << data[0] << "\n";
    std::cout << "Last element: " << data.back() << "\n";
    
    return 0;
}
