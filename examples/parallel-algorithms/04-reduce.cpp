#include <numeric>
#include <execution>
#include <vector>
#include <iostream>
#include <numeric>

int main() {
    std::vector<int> data(1000000);
    std::iota(data.begin(), data.end(), 1);
    
    // Sequential accumulate
    auto sum_seq = std::accumulate(data.begin(), data.end(), 0);
    std::cout << "Sequential sum: " << sum_seq << "\n";
    
    // Parallel reduce
    auto sum_par = std::reduce(std::execution::par, 
                              data.begin(), data.end(), 
                              0, std::plus<>{});
    std::cout << "Parallel sum: " << sum_par << "\n";
    
    return 0;
}
