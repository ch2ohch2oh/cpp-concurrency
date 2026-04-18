#include <thread>
#include <iostream>
#include <sstream>
#include "../common.h"

thread_local int thread_counter = 0;

void increment() {
    thread_counter++;
    std::ostringstream oss;
    oss << "Thread " << std::this_thread::get_id()
        << ": counter = " << thread_counter << "\n";
    print(oss.str());
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    std::thread t3(increment);

    increment();

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
