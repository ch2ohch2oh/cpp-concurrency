#include <atomic>
#include <iostream>

// bool compare_exchange_weak(T& expected, T desired,
//                           std::memory_order success,
//                           std::memory_order failure) noexcept;
//
// Parameters:
//   expected: Reference to expected value. On failure, updated with actual value
//   desired:  New value to store if comparison succeeds
//   success:  Memory order for when comparison succeeds (we performed a write)
//   failure:  Memory order for when comparison fails (we only performed a read)
//
// Why two memory orders?
//   - Success: We wrote a new value, so we need release semantics
//   - Failure: We only read the value, so we only need acquire semantics
//   - The failure order must be no stronger than the success order
//
// Returns: true if value was exchanged, false otherwise
// Note: May fail spuriously even when expected == value (use in loops)

std::atomic<int> value(10);

// Simple example: update value only if it's still 10
// Note: This is a single-threaded example for demonstration.
// In real multi-threaded code, memory orders are critical for correctness.
bool try_update() {
    int expected = 10;
    // If value == expected, set it to 20 and return true
    // If value != expected, update expected with actual value and return false
    
    // Using acq_rel/acquire because:
    // - acq_rel on success: ensures our write (20) is visible to other threads
    //   and we see all prior writes from other threads
    // - acquire on failure: ensures we see the most recent value written by
    //   the thread that actually modified the atomic variable
    //
    // In this single-threaded example, memory_order_relaxed would work too,
    // but these orders demonstrate proper usage for concurrent scenarios.
    return value.compare_exchange_weak(expected, 20,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire);
}

int main() {
    if (try_update()) {
        std::cout << "Successfully updated to 20\n";
    } else {
        std::cout << "Failed, current value is: " << value.load() << "\n";
    }
    return 0;
}
