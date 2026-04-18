#include <thread>
#include <random>
#include <iostream>
#include <sstream>
#include "../common.h"

thread_local std::mt19937 rng(std::random_device{}());

int random_int(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

void generate_numbers(int count) {
    std::ostringstream oss;
    oss << "Thread " << std::this_thread::get_id() << ": ";
    for (int i = 0; i < count; ++i) {
        oss << random_int(1, 100) << " ";
    }
    oss << "\n";
    print(oss.str());
}

int main() {
    std::thread t1(generate_numbers, 5);
    std::thread t2(generate_numbers, 5);
    std::thread t3(generate_numbers, 5);

    generate_numbers(5);

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
