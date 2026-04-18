# Lock-Free Programming: Usage and Implementation

## Practical Usage

### Lock-Free Stack

```cpp
#include <atomic>
#include <memory>

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        std::shared_ptr<Node> next;
    };
    
    std::atomic<std::shared_ptr<Node>> head_;
    
public:
    LockFreeStack() : head_(nullptr) {}
    
    void push(T value) {
        auto new_node = std::make_shared<Node>();
        new_node->data = value;
        
        auto old_head = head_.load(std::memory_order_relaxed);
        do {
            new_node->next = old_head;
        } while (!head_.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }
    
    std::shared_ptr<T> pop() {
        auto old_head = head_.load(std::memory_order_acquire);
        while (old_head && !head_.compare_exchange_weak(
            old_head, old_head->next,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            // Retry
        }
        
        if (old_head) {
            return std::make_shared<T>(old_head->data);
        }
        return nullptr;
    }
    
    bool empty() const {
        return head_.load(std::memory_order_relaxed) == nullptr;
    }
};
```

### Lock-Free Queue (Michael-Scott)

```cpp
#include <atomic>
#include <memory>

template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next;
        
        Node() : next(nullptr) {}
        Node(T value) : data(std::make_shared<T>(value)), next(nullptr) {}
    };
    
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    
public:
    LockFreeQueue() {
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }
    
    ~LockFreeQueue() {
        while (Node* head = head_.load()) {
            head_.store(head->next);
            delete head;
        }
    }
    
    void enqueue(T value) {
        Node* new_node = new Node(value);
        
        while (true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            
            if (last == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(
                        next, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                        tail_.compare_exchange_weak(
                            last, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(
                        last, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
        }
    }
    
    bool dequeue(T& result) {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            
            if (first == head_.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        return false;  // Queue is empty
                    }
                    tail_.compare_exchange_weak(
                        last, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                } else {
                    result = *next->data;
                    if (head_.compare_exchange_weak(
                        first, next,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                        delete first;
                        return true;
                    }
                }
            }
        }
    }
};
```

### Lock-Free Counter

```cpp
#include <atomic>
#include <vector>

class ShardedCounter {
private:
    std::vector<std::atomic<uint64_t>> counters_;
    static constexpr size_t num_shards_ = 64;
    
public:
    ShardedCounter() : counters_(num_shards_) {
        for (auto& c : counters_) {
            c.store(0, std::memory_order_relaxed);
        }
    }
    
    void increment() {
        size_t shard = std::hash<std::thread::id>{}(
            std::this_thread::get_id()) % num_shards_;
        counters_[shard].fetch_add(1, std::memory_order_relaxed);
    }
    
    uint64_t get() const {
        uint64_t total = 0;
        for (const auto& c : counters_) {
            total += c.load(std::memory_order_relaxed);
        }
        return total;
    }
};
```

### Lock-Free Reference Counting

```cpp
#include <atomic>

class LockFreeReferenceCounted {
private:
    mutable std::atomic<int> ref_count_;
    
public:
    LockFreeReferenceCounted() : ref_count_(1) {}
    
    void add_ref() const {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    void release() const {
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
    
    int ref_count() const {
        return ref_count_.load(std::memory_order_relaxed);
    }
};
```

### Realistic Example: Lock-Free Hash Table

```cpp
#include <atomic>
#include <vector>
#include <functional>

template<typename Key, typename Value>
class LockFreeHashTable {
private:
    struct Entry {
        Key key;
        Value value;
        std::atomic<Entry*> next;
        
        Entry(const Key& k, const Value& v) 
            : key(k), value(v), next(nullptr) {}
    };
    
    std::vector<std::atomic<Entry*>> buckets_;
    size_t size_;
    
    size_t hash(const Key& key) const {
        return std::hash<Key>{}(key) % size_;
    }
    
public:
    LockFreeHashTable(size_t size) : size_(size), buckets_(size) {
        for (auto& bucket : buckets_) {
            bucket.store(nullptr, std::memory_order_relaxed);
        }
    }
    
    bool insert(const Key& key, const Value& value) {
        size_t index = hash(key);
        Entry* new_entry = new Entry(key, value);
        
        while (true) {
            Entry* head = buckets_[index].load(std::memory_order_acquire);
            
            // Check if key already exists
            Entry* current = head;
            while (current) {
                if (current->key == key) {
                    delete new_entry;
                    return false;  // Key already exists
                }
                current = current->next.load(std::memory_order_acquire);
            }
            
            new_entry->next.store(head, std::memory_order_relaxed);
            if (buckets_[index].compare_exchange_weak(
                head, new_entry,
                std::memory_order_release,
                std::memory_order_relaxed)) {
                return true;
            }
        }
    }
    
    bool lookup(const Key& key, Value& result) const {
        size_t index = hash(key);
        Entry* current = buckets_[index].load(std::memory_order_acquire);
        
        while (current) {
            if (current->key == key) {
                result = current->value;
                return true;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        
        return false;
    }
};
```

## Underlying Implementation

### ABA Problem

The ABA problem occurs when a value changes from A to B and back to A:

```cpp
// ABA problem example
std::atomic<Node*> stack(nullptr);

// Thread 1 reads A
Node* old = stack.load();

// Thread 2 changes A -> B -> A
stack.compare_exchange_strong(old, new_node);
// Succeeds even though state changed

// Solution: Use versioned pointers (double-word CAS)
struct VersionedPointer {
    Node* ptr;
    uint64_t version;
};

std::atomic<VersionedPointer> stack;
```

### Double-Word Compare-Exchange

```cpp
// Using 128-bit CAS for versioned pointers
struct VersionedPointer {
    Node* ptr;
    uint64_t version;
    
    bool operator==(const VersionedPointer& other) const {
        return ptr == other.ptr && version == other.version;
    }
};

std::atomic<VersionedPointer> stack;

bool push(Node* new_node) {
    VersionedPointer old = stack.load(std::memory_order_acquire);
    VersionedPointer new_ptr{new_node, old.version + 1};
    
    return stack.compare_exchange_strong(
        old, new_ptr,
        std::memory_order_release,
        std::memory_order_acquire);
}
```

### Hazard Pointers

Hazard pointers solve the memory reclamation problem:

```cpp
class HazardPointer {
private:
    static constexpr int MAX_HAZARDS = 64;
    static thread_local std::atomic<void*> hazards[MAX_HAZARDS];
    static std::vector<void*> retired_list;
    static std::mutex retired_mtx;
    
public:
    static void acquire(void* ptr) {
        for (int i = 0; i < MAX_HAZARDS; ++i) {
            void* expected = nullptr;
            if (hazards[i].compare_exchange_strong(
                expected, ptr,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
                return;
            }
        }
        throw std::runtime_error("Too many hazards");
    }
    
    static void release(void* ptr) {
        for (int i = 0; i < MAX_HAZARDS; ++i) {
            void* expected = ptr;
            if (hazards[i].compare_exchange_strong(
                expected, nullptr,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
                return;
            }
        }
    }
    
    static void retire(void* ptr) {
        std::lock_guard<std::mutex> lock(retired_mtx);
        retired_list.push_back(ptr);
        
        // Check if any hazard points to retired pointer
        bool safe = true;
        for (int i = 0; i < MAX_HAZARDS; ++i) {
            if (hazards[i].load() == ptr) {
                safe = false;
                break;
            }
        }
        
        if (safe) {
            delete static_cast<Node*>(ptr);
        }
    }
};
```

### Epoch-Based Reclamation

```cpp
class EpochManager {
private:
    static constexpr size_t NUM_EPOCHS = 3;
    static std::atomic<size_t> current_epoch_;
    static std::array<std::vector<void*>, NUM_EPOCHS> retired_;
    static thread_local size_t local_epoch_;
    static std::mutex retired_mtx_;
    
public:
    static void enter() {
        local_epoch_ = current_epoch_.load(std::memory_order_acquire);
    }
    
    static void exit() {
        local_epoch_ = -1;
    }
    
    static void retire(void* ptr) {
        size_t epoch = current_epoch_.load(std::memory_order_acquire);
        size_t retire_epoch = (epoch + 1) % NUM_EPOCHS;
        
        std::lock_guard<std::mutex> lock(retired_mtx_);
        retired_[retire_epoch].push_back(ptr);
        
        // Try to advance epoch
        if (retired_[epoch].empty()) {
            current_epoch_.compare_exchange_strong(
                epoch, (epoch + 1) % NUM_EPOCHS,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
            
            // Reclaim from two epochs ago
            size_t reclaim_epoch = (epoch + 2) % NUM_EPOCHS;
            for (void* p : retired_[reclaim_epoch]) {
                delete static_cast<Node*>(p);
            }
            retired_[reclaim_epoch].clear();
        }
    }
};
```

### Read-Copy-Update (RCU)

```cpp
class RCU {
private:
    static std::atomic<int> readers_;
    static std::vector<void*> to_free_;
    static std::mutex mtx_;
    
public:
    static void read_lock() {
        readers_.fetch_add(1, std::memory_order_acquire);
    }
    
    static void read_unlock() {
        readers_.fetch_sub(1, std::memory_order_release);
    }
    
    static void update(void* old_ptr, void* new_ptr) {
        // Wait for all readers to finish
        while (readers_.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }
        
        // Swap pointers
        std::atomic<void*>& global_ptr = /* ... */;
        void* expected = old_ptr;
        global_ptr.compare_exchange_strong(
            expected, new_ptr,
            std::memory_order_release,
            std::memory_order_acquire);
        
        // Schedule old_ptr for deletion
        std::lock_guard<std::mutex> lock(mtx_);
        to_free_.push_back(old_ptr);
    }
    
    static void reclaim() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (void* ptr : to_free_) {
            delete static_cast<Node*>(ptr);
        }
        to_free_.clear();
    }
};
```

### Progress Guarantees

```cpp
// Lock-free: At least one thread makes progress
class LockFreeCounter {
    std::atomic<int> count_;
public:
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }
};

// Wait-free: All threads make progress
class WaitFreeCounter {
    std::atomic<int> count_;
public:
    void increment() {
        // Always completes in bounded time
        count_.fetch_add(1, std::memory_order_relaxed);
    }
};

// Obstruction-free: If running alone, completes
class ObstructionFreeCounter {
    std::atomic<int> count_;
public:
    void increment() {
        int expected = count_.load();
        while (!count_.compare_exchange_weak(
            expected, expected + 1,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
            // Retry if obstructed
        }
    }
};
```

### Backoff Strategies

```cpp
// Exponential backoff
void exponential_backoff(int attempt) {
    int delay = 1 << std::min(attempt, 10);
    for (int i = 0; i < delay; ++i) {
        std::this_thread::yield();
    }
}

// Fixed backoff
void fixed_backoff() {
    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
}

// Adaptive backoff
class AdaptiveBackoff {
private:
    int backoff_;
    int success_count_;
    
public:
    AdaptiveBackoff() : backoff_(1), success_count_(0) {}
    
    void on_success() {
        success_count_++;
        if (success_count_ > 2 && backoff_ > 1) {
            backoff_ /= 2;
            success_count_ = 0;
        }
    }
    
    void on_failure() {
        success_count_ = 0;
        backoff_ = std::min(backoff_ * 2, 1000);
        for (int i = 0; i < backoff_; ++i) {
            std::this_thread::yield();
        }
    }
};
```

### Cache Line Padding

```cpp
// Prevent false sharing
struct alignas(64) PaddedAtomic {
    std::atomic<uint64_t> value_;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
};

class ShardedCounter {
    std::vector<PaddedAtomic> counters_;
public:
    void increment() {
        size_t shard = /* ... */;
        counters_[shard].value_.fetch_add(1, std::memory_order_relaxed);
    }
};
```

## Best Practices

1. Start with lock-based solutions, optimize to lock-free only if needed
2. Use proven algorithms (Michael-Scott queue, Treiber stack)
3. Handle the ABA problem with versioned pointers
4. Use hazard pointers or epoch-based reclamation for memory management
5. Implement backoff strategies for high contention
6. Pad structures to cache line size to prevent false sharing
7. Test thoroughly under high contention
8. Consider lock-free only for performance-critical code
9. Understand the difference between lock-free and wait-free
10. Use tools like ThreadSanitizer to detect issues
