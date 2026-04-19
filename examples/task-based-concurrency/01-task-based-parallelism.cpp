#include <future>
#include <vector>
#include <numeric>
#include <iostream>

std::future<int> compute_sum(const std::vector<int>& data) {
    return std::async(std::launch::async, [&data]() {
        return std::accumulate(data.begin(), data.end(), 0);
    });
}

int main() {
    std::vector<int> data1(1000, 1);
    std::vector<int> data2(1000, 2);
    
    auto future1 = compute_sum(data1);
    auto future2 = compute_sum(data2);
    
    int sum = future1.get() + future2.get();
    std::cout << "Total sum: " << sum << "\n";
    
    return 0;
}
