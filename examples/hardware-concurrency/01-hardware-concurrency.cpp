#include <thread>
#include <iostream>

int main() {
    unsigned int concurrency = std::thread::hardware_concurrency();
    std::cout << "Hardware concurrency: " << concurrency << "\n";
    return 0;
}
