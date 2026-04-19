#include <future>
#include <algorithm>
#include <vector>
#include <numeric>
#include <iostream>

template<typename It, typename F>
auto parallel_fork_join(It first, It last, F func, size_t threshold = 1000)
    -> std::future<decltype(func(first, last))> {
    
    using ReturnType = decltype(func(first, last));
    
    auto size = std::distance(first, last);
    if (size < threshold) {
        std::promise<ReturnType> prom;
        std::future<ReturnType> fut = prom.get_future();
        prom.set_value(func(first, last));
        return fut;
    }
    
    It mid = first + size / 2;
    
    auto left = parallel_fork_join(first, mid, func, threshold);
    auto right = parallel_fork_join(mid, last, func, threshold);
    
    return std::async(std::launch::async, [left = std::move(left), 
                                             right = std::move(right), func]() mutable {
        return func(left.get(), right.get());
    });
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    auto sum_future = parallel_fork_join(
        data.begin(), data.end(),
        [](auto first, auto last) {
            return std::accumulate(first, last, 0);
        }
    );
    
    std::cout << "Sum: " << sum_future.get() << "\n";
    return 0;
}
