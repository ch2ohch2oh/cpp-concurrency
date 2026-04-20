#include <algorithm>
#include <execution>
#include <vector>
#include <cmath>
#include <numeric>
#include <iostream>

int main() {
    std::vector<double> input(1000000);
    std::vector<double> output(1000000);
    
    std::iota(input.begin(), input.end(), 0.0);

    // Sequential transform
    std::transform(input.begin(), input.end(),
                  output.begin(),
                  [](double x) { return std::sqrt(x); });

#if __cpp_lib_parallel_algorithms >= 201603
    // Parallel transform
    std::transform(std::execution::par,
                  input.begin(), input.end(),
                  output.begin(),
                  [](double x) { return std::sqrt(x); });
#else
    std::cout << "Parallel algorithms not available (requires C++17 with library support)\n";
#endif

    return 0;
}
