#include <semaphore>
#include <thread>
#include <iostream>

// Binary semaphore: Unlike mutex, has NO ownership semantics
// - Any thread can release (not just the one that acquired)
// - No recursive locking (deadlocks if same thread acquires twice)
// - Can be used for signaling between threads (this example)
// - Generally prefer mutex for mutual exclusion, use semaphore for signaling

std::binary_semaphore data_ready(0);  // Start at 0 (no data ready)
bool data_available = false;

void producer() {
    std::cout << "Producer: preparing data\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    data_available = true;
    std::cout << "Producer: signaling data ready\n";
    data_ready.release();  // Signal that data is ready
}

void consumer() {
    std::cout << "Consumer: waiting for data\n";
    data_ready.acquire();  // Wait for signal from producer
    std::cout << "Consumer: received signal, processing data\n";
}

int main() {
    std::thread p(producer);
    std::thread c(consumer);

    p.join();
    c.join();

    return 0;
}
