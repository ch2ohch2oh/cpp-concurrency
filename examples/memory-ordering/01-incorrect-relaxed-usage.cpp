#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> data_ready(false);
int data = 0;
int failures = 0;

void producer() {
    // Loop assigning 1-100 - compiler/CPU may reorder stores within loop
    for (int i = 1; i <= 100; i++) {
        data = i;
    }
    
    data_ready.store(true, std::memory_order_relaxed);  // BUG: Wrong memory order
}

void consumer() {
    while (!data_ready.load(std::memory_order_relaxed)) {
        // Spin
    }
    // BUG: If reordering occurs, flag might be visible before loop completes
    // So data might be < 100 instead of 100
    int data_value = data;
    if (data_value != 100) {
        failures++;
        std::cout << "Failure detected: data=" << data_value << " (should be 100)\n";
    }
}

int main() {
    const int iterations = 100'000;
    
    std::cout << "Running " << iterations << " iterations...\n";
    std::cout << "Note: This bug is more likely to occur on weak memory architectures (ARM, PowerPC)\n";
    std::cout << "On x86 with TSO, it may rarely fail due to strong ordering guarantees.\n";
    std::cout << "The loop (1-100) increases chance of store reordering within the loop.\n\n";
    
    for (int i = 0; i < iterations; i++) {
        data_ready = false;
        data = 0;
        
        std::thread c(consumer);
        std::thread p(producer);
        p.join();
        c.join();
    }
    
    std::cout << "Failures detected: " << failures << " / " << iterations << "\n";
    
    if (failures > 0) {
        std::cout << "BUG REPRODUCED: Memory ordering issue detected!\n";
        std::cout << "Fix: Use memory_order_release for store and memory_order_acquire for load.\n";
    } else {
        std::cout << "No failures detected. This may be due to:\n";
        std::cout << "  - Strong memory model (x86 TSO)\n";
        std::cout << "  - Insufficient iterations\n";
        std::cout << "  - CPU not reordering this particular pattern\n";
        std::cout << "Try running on ARM or increasing iterations.\n";
    }
    
    return 0;
}
