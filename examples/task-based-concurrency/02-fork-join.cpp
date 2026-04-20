#include <future>
#include <algorithm>
#include <vector>
#include <numeric>
#include <iostream>

// Simple fork-join for summing a range
int parallel_sum(std::vector<int>::iterator first, std::vector<int>::iterator last, size_t threshold = 1000) {
    auto size = std::distance(first, last);
    if (size < threshold) {
        return std::accumulate(first, last, 0);
    }
    
    auto mid = first + size / 2;
    
    auto left = std::async(std::launch::async, parallel_sum, first, mid, threshold);
    auto right = std::async(std::launch::async, parallel_sum, mid, last, threshold);
    
    return left.get() + right.get();
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    auto sum = parallel_sum(data.begin(), data.end());
    std::cout << "Sum: " << sum << "\n";
    return 0;
}
