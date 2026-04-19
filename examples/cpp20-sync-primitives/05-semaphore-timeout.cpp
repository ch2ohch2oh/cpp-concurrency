#include <semaphore>
#include <thread>
#include <chrono>
#include <iostream>

std::counting_semaphore<1> sem(0);

void producer() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    sem.release();
    std::cout << "Producer released semaphore\n";
}

void consumer() {
    std::cout << "Consumer waiting...\n";
    
    if (sem.try_acquire_for(std::chrono::seconds(1))) {
        std::cout << "Consumer acquired semaphore\n";
    } else {
        std::cout << "Consumer timeout\n";
    }
}

int main() {
    std::thread p(producer);
    std::thread c(consumer);
    
    p.join();
    c.join();
    
    return 0;
}
