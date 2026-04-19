#include <algorithm>
#include <execution>
#include <vector>
#include <iostream>
#include <numeric>

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 0);
    
    // Parallel count_if
    auto count = std::count_if(std::execution::par, 
                               data.begin(), data.end(),
                               [](int x) { return x % 2 == 0; });
    
    std::cout << "Even numbers: " << count << "\n";
    
    return 0;
}
