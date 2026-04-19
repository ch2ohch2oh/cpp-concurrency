#include <algorithm>
#include <execution>
#include <vector>
#include <cmath>
#include <numeric>

int main() {
    std::vector<double> input(1000000);
    std::vector<double> output(1000000);
    
    std::iota(input.begin(), input.end(), 0.0);
    
    // Parallel transform
    std::transform(std::execution::par, 
                  input.begin(), input.end(), 
                  output.begin(),
                  [](double x) { return std::sqrt(x); });
    
    return 0;
}
