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
    
    // Parallel for_each
    std::for_each(std::execution::par, data.begin(), data.end(), process_item);
    
    std::cout << "First element: " << data[0] << "\n";
    std::cout << "Last element: " << data.back() << "\n";
    
    return 0;
}
