#include <atomic>
#include <iostream>

int main() {
    int value = 10;
    std::atomic_ref<int> ref(value);
    
    int expected = 10;
    int desired = 20;
    
    if (ref.compare_exchange_strong(expected, desired)) {
        std::cout << "Exchange successful\n";
    } else {
        std::cout << "Exchange failed, expected: " << expected << "\n";
    }
    
    std::cout << "Final value: " << value << "\n";
    return 0;
}
