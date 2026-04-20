#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>
#include <numeric>

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 0);

    // Sequential count_if
    auto count = std::count_if(data.begin(), data.end(),
                               [](int x) { return x % 2 == 0; });

    std::cout << "Even numbers: " << count << "\n";

#if __cpp_lib_parallel_algorithms >= 201603
    // Parallel count_if
    auto count_par = std::count_if(std::execution::par,
                                   data.begin(), data.end(),
                                   [](int x) { return x % 2 == 0; });
    std::cout << "Parallel even numbers: " << count_par << "\n";
#else
    std::cout << "Parallel algorithms not available (requires C++17 with library support)\n";
#endif

    return 0;
}
