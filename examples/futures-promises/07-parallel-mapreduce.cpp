#include <future>
#include <vector>
#include <numeric>
#include <iostream>

template<typename Iterator, typename Func>
auto parallel_map(Iterator begin, Iterator end, Func f) 
    -> std::vector<decltype(f(*begin))> {
    
    using ResultType = decltype(f(*begin));
    std::vector<std::future<ResultType>> futures;
    
    for (auto it = begin; it != end; ++it) {
        futures.push_back(std::async(std::launch::async, [f, it] {
            return f(*it);
        }));
    }
    
    std::vector<ResultType> results;
    for (auto& fut : futures) {
        results.push_back(fut.get());
    }
    
    return results;
}

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5};
    
    auto results = parallel_map(data.begin(), data.end(), [](int x) {
        return x * x;
    });
    
    int sum = std::accumulate(results.begin(), results.end(), 0);
    std::cout << "Sum of squares: " << sum << "\n";
    
    return 0;
}
