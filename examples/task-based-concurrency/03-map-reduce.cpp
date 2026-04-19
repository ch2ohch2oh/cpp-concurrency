#include <future>
#include <vector>
#include <algorithm>
#include <numeric>
#include <iostream>

template<typename InputIt, typename MapFunc, typename ReduceFunc>
auto map_reduce(InputIt first, InputIt last, 
                MapFunc map, ReduceFunc reduce,
                size_t chunk_size = 1000) 
    -> typename std::result_of<MapFunc decltype(*first)>::type {
    
    using MappedType = typename std::result_of<MapFunc decltype(*first)>::type;
    
    std::vector<std::future<MappedType>> futures;
    
    for (auto it = first; it < last; it += chunk_size) {
        auto end = std::min(it + chunk_size, last);
        
        futures.push_back(std::async(std::launch::async, [=]() {
            MappedType result = map(*it);
            for (auto i = it + 1; i != end; ++i) {
                result = reduce(result, map(*i));
            }
            return result;
        }));
    }
    
    MappedType final_result = futures[0].get();
    for (size_t i = 1; i < futures.size(); ++i) {
        final_result = reduce(final_result, futures[i].get());
    }
    
    return final_result;
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    auto result = map_reduce(
        data.begin(), data.end(),
        [](int x) { return x * x; },  // Map
        [](int a, int b) { return a + b; }  // Reduce
    );
    
    std::cout << "Sum of squares: " << result << "\n";
    return 0;
}
