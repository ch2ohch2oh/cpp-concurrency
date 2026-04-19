# Lock-Free Programming: Usage and Implementation

## History

Lock-free programming has evolved significantly over the past several decades:

**1986 - Treiber's Algorithm**
- R. Kent Treiber publishes "Systems Programming: Coping with Parallelism"
- Introduces the first practical lock-free stack using compare-and-swap
- Foundation for many subsequent lock-free data structures

**1990s - Theoretical Foundations**
- Herlihy and Wait-Free Hierarchy (1991): Defines progress guarantees (obstruction-free, lock-free, wait-free)
- Introduction of the ABA problem and solutions
- Development of universal constructions for lock-free algorithms

**1996 - Michael-Scott Queue**
- Maged Michael and Michael Scott publish the lock-free queue algorithm
- Becomes the standard for concurrent queue implementations
- Used extensively in production systems (Java ConcurrentLinkedQueue, etc.)

**2000s - Memory Reclamation**
- Hazard pointers (2002): Michael provides safe memory reclamation
- Epoch-based reclamation: Development of efficient batch reclamation
- RCU (Read-Copy-Update): Popularized in Linux kernel, adopted elsewhere

**2011 - C++11 Standardization**
- First standard support for atomic operations and memory ordering
- `std::atomic` provides portable atomic primitives
- Memory ordering model formalized (acquire, release, relaxed, etc.)

**2017 - C++17 Improvements**
- Enhanced atomic operations
- Better support for lock-free programming patterns

**2020 - C++20**
- `std::atomic_ref` for atomic operations on existing objects
- `std::atomic_shared_ptr` and `std::atomic_weak_ptr`
- Improved atomic wait/notify operations

**Modern Era**
- Lock-free data structures in major libraries (Boost.Lockfree, Folly)
- Widespread use in high-performance systems (databases, network stacks)
- Research continues on wait-free algorithms and practical implementations

## When to Use Lock-Free Programming

**General Rule:** Avoid lock-free programming unless absolutely necessary. Start with locks and profile first.

### Decision Hierarchy

**1. Use higher-level abstractions first:**
- `std::mutex`, `std::condition_variable`, thread-safe queues
- Lock-based solutions are easier to reason about and debug
- Use libraries like Intel TBB, concurrent containers

**2. Consider atomics when:**
- You've measured a performance bottleneck with locks
- Contention is high and locks are causing serialization
- You need wait-free or lock-free guarantees (real-time systems)
- Simple cases like counters, flags, or single shared variable

**3. Lock-free data structures when:**
- Profiling shows locks are the bottleneck
- You have expertise or can use proven libraries
- Read-mostly workloads (RCU) or high-contention scenarios

### Why Avoid Lock-Free Programming?

- **Complexity:** Lock-free code is 10x harder to get right
- **Debugging:** Bugs are subtle and hard to reproduce
- **Optimized locks:** Modern mutexes are highly optimized (futex, adaptive spinning)
- **Diminishing returns:** Most applications don't need lock-free for good performance

### When Lock-Free is Worth It

- High-contention scenarios where locks serialize too much
- Real-time systems with latency guarantees
- Read-mostly data structures (RCU shines here)
- Shared counters/metrics accessed by many threads
- When using proven, well-tested libraries

## Practical Usage

### Lock-Free Stack

**Use Cases:**
- Task scheduling and work queues where multiple threads push/pop tasks
- Undo/redo stacks in concurrent applications
- Expression evaluation in parallel compilers

**Pros:**
- Simple implementation using Treiber's algorithm (a lock-free stack algorithm using CAS on head pointer)
- No locks means no deadlock risk
- Good for scenarios with moderate contention
- O(1) push and pop operations

**Cons:**
- Memory reclamation is complex (ABA problem)
- Can suffer from high contention under heavy load
- May require backoff strategies for performance
- Not wait-free (threads may starve)

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
    
    // NOTE: empty() is unreliable in lock-free contexts. The result may be
    // stale immediately after returning due to concurrent push/pop operations.
    // Never use empty() to guard pop() - just call pop() and handle nullptr.
};
```

### Lock-Free Queue (Michael-Scott)

**Use Cases:**
- Producer-consumer patterns with multiple producers/consumers
- Message passing between threads
- Event processing pipelines
- Network packet processing

**Pros:**
- Proven algorithm with correct synchronization
- Supports multiple concurrent producers and consumers
- No locks, avoiding priority inversion
- Scalable for moderate contention

**Cons:**
- More complex than stack implementation
- Requires dummy node, adding memory overhead
- Memory reclamation is challenging
- CAS loops can cause cache contention
- Not wait-free

**Note on Correctness:** This algorithm has been formally proved correct in the 1996 paper "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms" by Michael and Scott. The proof covers linearizability, lock-freedom, and freedom from ABA problems. This mathematical verification is why it's safe to use in production, unlike custom lock-free implementations which often have subtle bugs.

**Complexity Warning:** Despite being called "simple" in the paper title, this algorithm is significantly more complex than the lock-free stack. It requires understanding the two-step CAS pattern, help mechanisms, and subtle consistency checks. The 30-page formal proof demonstrates the complexity - this is not something to implement from scratch without deep expertise.

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
        // WARNING: This destructor is NOT thread-safe. It assumes no other threads
        // are accessing the queue during destruction. A production implementation
        // would need a shutdown flag, safe memory reclamation (hazard pointers or
        // epoch-based reclamation), and proper synchronization to ensure all threads
        // have stopped using the queue before cleanup.
        while (Node* head = head_.load()) {
            head_.store(head->next);
            delete head;
        }
    }
    
    void enqueue(T value) {
        Node* new_node = new Node(value);
        
        while (true) {
            // Read tail and its next pointer
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            
            // Check if tail is still consistent (no other thread changed it)
            if (last == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Tail is at the end, try to link new node
                    if (last->next.compare_exchange_weak(
                        next, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                        // Successfully linked, now advance tail
                        tail_.compare_exchange_weak(
                            last, new_node,
                            std::memory_order_release,
                            std::memory_order_relaxed);
                        break;
                    }
                } else {
                    // Another thread is in the middle of enqueueing,
                    // help by advancing tail to next
                    tail_.compare_exchange_weak(
                        last, next,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                }
            }
            // If tail changed, retry from the beginning
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

**Use Cases:**
- Statistics collection (request counts, metrics)
- Reference counting for shared resources
- Performance counters and profiling
- Event counting in high-throughput systems

**Real-World Examples:**
- **Web Servers:** Nginx/Apache request counters (10K-100K+ req/sec per server)
- **Databases:** PostgreSQL transaction stats, Redis key hit/miss counters
- **Cloud:** AWS Lambda invocation tracking, Kubernetes pod metrics
- **Monitoring:** Prometheus scrape counters, StatsD metric aggregation
- **Network:** Load balancer request distribution, router packet counters
- **Big Data:** Kafka offset tracking, Spark task counters

**Update Frequencies:**
- Threshold: ~1 million increments/second on a single counter causes bottlenecks
- High-traffic CDNs: Millions of requests/second across edge locations
- Big data pipelines: Millions of messages/second with per-partition counters
- The need for sharding grows with core count and update frequency

**Pros:**
- Sharding reduces contention significantly
- Simple to implement and understand
- Wait-free operations (bounded time)
- Excellent scalability with many threads
- No locks or CAS loops required

**Cons:**
- `get()` operation is O(n) over shards
- Reads are not atomic with writes
- Memory overhead from multiple counters
- May show stale counts temporarily

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

**Use Cases:**
- Shared object lifetime management
- Smart pointer implementations
- Resource cleanup in concurrent systems
- Caching systems with shared entries

**Pros:**
- No locks needed for reference management
- Atomic operations ensure thread safety
- Simple and lightweight
- Works well for read-heavy workloads

**Cons:**
- ABA problem if not handled carefully
- Memory leaks if cycles exist
- Can be slower than lock-based for low contention
- Requires careful memory ordering

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

**Use Cases:**
- Concurrent caches and memoization
- Symbol tables in parallel compilers
- Index structures in databases
- Routing tables in network systems

**Pros:**
- High concurrency for read/write operations
- No global locks, good scalability
- Can handle high throughput
- Lookup is lock-free

**Cons:**
- Very complex to implement correctly
- Resize/rehash is extremely challenging
- Memory reclamation is difficult
- May have higher constant overhead
- Limited operations (no complex transactions)

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

### Compare-Exchange Weak (`compare_exchange_weak`)

`compare_exchange_weak` is the core primitive for lock-free algorithms. It atomically compares and exchanges values:

```cpp
bool compare_exchange_weak(T& expected, T desired,
                           std::memory_order success,
                           std::memory_order failure);
```

**How it works:**
1. Atomically reads the current value of the atomic variable
2. If the current value equals `expected`, it replaces it with `desired` and returns `true`
3. If not equal, it updates `expected` with the actual current value and returns `false`
4. The "weak" variant may fail spuriously (return false even if values match) due to hardware limitations

**Why "weak" can fail spuriously:**
- On some architectures (e.g., ARM, PowerPC), CAS is implemented as LL/SC (Load-Linked/Store-Conditional)
- LL/SC can fail if the cache line is written by any processor between load and store
- This can happen even if the value hasn't changed (e.g., unrelated write to same cache line)
- The weak variant allows this spurious failure; the strong variant retries automatically

**When to use weak vs strong:**
- **Weak**: Use in loops (you'll retry anyway). More efficient on some architectures. May fail spuriously even if values match.
- **Strong**: Use when you need exactly one attempt or can't retry. Guaranteed not to fail spuriously - if it fails, the value actually changed. May be less efficient on LL/SC architectures as it may need internal retries.

**Example pattern:**
```cpp
auto expected = atomic_var.load(std::memory_order_relaxed);
while (!atomic_var.compare_exchange_weak(
    expected, desired,
    std::memory_order_release,
    std::memory_order_relaxed)) {
    // expected is automatically updated with actual value
    // Loop continues until CAS succeeds
}
```

**Memory ordering:**
- `success`: Ordering used when exchange succeeds
- `failure`: Ordering used when exchange fails (must be no stronger than `success`)
- Common pattern: `release` for success, `relaxed` for failure

### ABA Problem

The ABA problem occurs when a value changes from A to B and back to A:

```
Timeline showing the ABA problem:

Initial state:  Stack -> [A] -> [C] -> [D] -> nullptr

Thread 1: Reads head = A, intends to pop A and set head to C
          (paused before CAS)

Thread 2: Pops A, then pushes B, then pushes A again
          Stack -> [B] -> [C] -> [D] -> nullptr
          Stack -> [A] -> [B] -> [C] -> [D] -> nullptr

Thread 1: Resumes, CAS(old_head=A, new_head=C)
          Succeeds because head still equals A!
          But now C is not actually the next node after A
          Stack -> [C] -> [D] -> nullptr
          B is lost! Memory leak or corruption.

The problem: Thread 1's CAS succeeds even though the state changed,
because the value "looks the same" (A) but the structure is different.
```

**Historical Context:**
- **1970s-1980s:** First discovered in early multiprocessor systems (IBM System/370) and lock-free algorithm research
- **Why hard to find:** Timing-dependent, rare in testing, subtle corruption that works 99.9% of the time
- **Real-world impact:** Database list corruption, kernel data structure bugs, network queue packet loss
- **Solutions developed:** Hazard pointers (2002), epoch-based reclamation, versioned pointers
- **Modern tools:** ThreadSanitizer and other race detectors can help identify ABA issues

**Use Cases:**
- Any lock-free data structure using pointers
- Stack and queue implementations
- Linked list operations

**Pros of Solution (Versioned Pointers):**
- Completely eliminates ABA problem
- Works with standard CAS operations
- Minimal overhead (just version counter)

**Cons:**
- Requires double-width CAS (128-bit on 64-bit systems)
- Not all architectures support double CAS
- Version counter can overflow (use 64-bit)
- Increases memory usage

**How Versioned Pointers Work:**
The key insight: even if a pointer value changes from A→B→A, the version number increments each time, so the full (ptr, version) pair is different:

```
Initial:  (ptr=A, version=1)
Thread 1 reads: (ptr=A, version=1)
Thread 2 changes: (ptr=B, version=2) → (ptr=A, version=3)
Thread 1 CAS: expected (A,1) vs actual (A,3) → FAILS!
```

The CAS compares both the pointer AND the version. Since the version changed (1→3), even though the pointer is the same (A), the CAS fails, preventing the ABA problem.

**When to Increment the Version:**
The version should increment every time the pointer value changes in a successful atomic operation:

```cpp
bool push(Node* new_node) {
    VersionedPointer old = stack.load();
    VersionedPointer new_ptr{new_node, old.version + 1};  // Increment before CAS
    return stack.compare_exchange_strong(old, new_ptr);
}
```

- Increment **before** the CAS attempt
- If CAS fails, the incremented version is discarded (no harm)
- If CAS succeeds, the new version is stored atomically with the new pointer
- This ensures every successful pointer change has a unique version number

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

**Use Cases:**
- Implementing versioned pointers for ABA prevention
- Atomic updates of paired values
- Complex state machines

**Pros:**
- Enables atomic updates of two values
- Essential for versioned pointer schemes
- Available on x86-64 and ARM64

**Cons:**
- Limited hardware support (x86-64, ARM64)
- Not portable to all architectures
- Performance overhead compared to single CAS
- Alignment requirements (16-byte aligned)

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

The Memory Reclamation Problem:
```
Initial: Stack -> [A] -> [B] -> [C] -> nullptr

Thread 1: Reads head = A (intends to pop A)
          Stores A in a local variable for processing
          (paused before deleting A)

Thread 2: Pops A, deletes it immediately
          Stack -> [B] -> [C] -> nullptr
          Node A is freed and memory reused for other purposes

Thread 1: Resumes, tries to access A->next
          CRASH! Use-after-free - A was already deleted
```

The problem: You can't delete a node immediately after removing it from
the data structure because other threads might still be accessing it.

**Note:** This is a general problem for ALL lock-free data structures (stacks, queues,
linked lists, hash tables, trees) - not specific to stacks. Any time a node is removed
from a lock-free structure while other threads might still reference it, you need safe
memory reclamation.

**Example with Raw Pointer Stack:**
```cpp
// Simplified stack using raw pointers (no safe memory reclamation)
struct Node { T data; Node* next; };
std::atomic<Node*> head;

// Thread 1: pop()
Node* old_head = head.load();  // Reads A
// Thread 1 preempted here!

// Thread 2: pop()
Node* old = head.load();       // Reads A
head.compare_exchange(old, A->next);  // Success, head now points to B
delete old;  // Deletes A immediately

// Thread 1: resumes
T result = old_head->data;  // CRASH! old_head points to freed memory
```

The current stack example in these notes uses `std::atomic<std::shared_ptr<Node>>`
which handles memory reclamation automatically via reference counting. But many
high-performance lock-free implementations use raw pointers and require hazard
pointers or epoch-based reclamation.

Hazard Pointer Solution:

```
Thread 1: Before reading A, announces "I'm accessing A" (sets hazard pointer)
          Reads head = A
          (paused)

Thread 2: Pops A, tries to delete it
          Checks hazard pointers - sees Thread 1 is accessing A
          Cannot delete A yet, adds to retired list

Thread 1: Finishes accessing A, clears hazard pointer
          (safe to delete A now)

Thread 2: On next retire operation, checks if A is still in any hazard pointer
          No hazards found - safe to delete A
```

**Use Cases:**
- Memory reclamation in lock-free data structures
- Safe deletion of nodes in concurrent lists
- Any lock-free structure with dynamic memory

**Pros:**
- No global synchronization needed
- Only retired nodes are checked
- Low overhead for read-heavy workloads
- Proven technique (used in Java, .NET)

**Cons:**
- Per-thread overhead for hazard arrays
- Retired list can grow large
- Requires periodic scanning
- More complex than simple locking
- Can have memory leaks if not managed

**MAX_HAZARDS:**
- Maximum number of hazard pointers a single thread can have active at once (typically 64)
- Each thread has its own array of hazard pointers (thread_local)
- Limits memory overhead (64 pointers × 8 bytes = 512 bytes per thread)
- Most algorithms need only 1-2 concurrent hazards (stack: 1, queue: 2)
- If exceeded, throws an exception - prevents unbounded memory growth

```cpp
class HazardPointer {
private:
    static constexpr int MAX_HAZARDS = 64;
    static thread_local std::atomic<void*> hazards[MAX_HAZARDS];
    static std::vector<void*> retired_list;
    static std::mutex retired_mtx;
    
public:
    // Mark a pointer as being accessed by this thread (announce "don't delete this")
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

    // Clear the hazard mark for a pointer (done accessing it)
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

    // Attempt to safely reclaim a pointer that's no longer needed
    // Checks if any thread has marked it as hazardous before deleting
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

**Example: Lock-Free Stack with Hazard Pointers:**
```cpp
template<typename T>
class LockFreeStackWithHazards {
    struct Node { T data; Node* next; };
    std::atomic<Node*> head;

    void push(T value) {
        Node* new_node = new Node{value, nullptr};
        Node* old_head = head.load();
        do {
            new_node->next = old_head;
        } while (!head.compare_exchange_weak(old_head, new_node));
    }

    std::shared_ptr<T> pop() {
        Node* old_head;
        while (true) {
            old_head = head.load();
            if (!old_head) return nullptr;
            HazardPointer::acquire(old_head);  // "I'm accessing this node"

            // Re-read to ensure consistency
            Node* current = head.load();
            if (current != old_head) {
                HazardPointer::release(old_head);  // Changed, retry
                continue;
            }

            if (head.compare_exchange_weak(old_head, old_head->next)) {
                break;  // Success
            }
            HazardPointer::release(old_head);  // CAS failed, retry
        }

        T result = old_head->data;
        HazardPointer::release(old_head);  // Done accessing
        HazardPointer::retire(old_head);    // Safe to delete now
        return std::make_shared<T>(result);
    }
};
```

### Epoch-Based Reclamation

Epoch-based reclamation divides time into "epochs" (time periods). Threads announce which epoch they're operating in, and retired memory is held until all threads have left that epoch (grace period). Only then is it safe to reclaim.

**How it works:**
- Time is divided into epochs (e.g., epoch 0, 1, 2)
- Threads call `enter()` to record the current epoch when starting an operation
- Threads call `exit()` when done
- Retired nodes are queued for deletion in a future epoch
- When all threads have left an epoch, nodes from two epochs ago can be safely deleted
- No per-pointer tracking - just epoch numbers

**Key insight:** Instead of tracking individual pointers (hazard pointers), track time periods. If a thread was in epoch 1 when it read a node, and we're now in epoch 3, the thread is definitely done with that node.

**Use Cases:**
- High-performance lock-free data structures
- Systems with many threads and frequent updates
- Read-mostly workloads with occasional writes

**Pros:**
- Very low overhead for readers
- Batch reclamation is efficient
- No per-node overhead
- Scales well to many threads
- Used in Linux kernel (RCU)

**Cons:**
- Requires grace period tracking
- Can delay reclamation significantly
- Writes have higher overhead
- Complex to implement correctly
- May cause memory pressure

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

            // Reclaim from epoch NUM_EPOCHS-1 steps ahead (one full cycle back)
            size_t reclaim_epoch = (epoch + NUM_EPOCHS - 1) % NUM_EPOCHS;
            for (void* p : retired_[reclaim_epoch]) {
                delete static_cast<Node*>(p);
            }
            retired_[reclaim_epoch].clear();
        }
    }
};
```

**Example: Lock-Free Stack with Epoch-Based Reclamation:**
```cpp
template<typename T>
class LockFreeStackWithEpoch {
    struct Node { T data; Node* next; };
    std::atomic<Node*> head;

    void push(T value) {
        Node* new_node = new Node{value, nullptr};
        Node* old_head = head.load();
        do {
            new_node->next = old_head;
        } while (!head.compare_exchange_weak(old_head, new_node));
    }

    std::shared_ptr<T> pop() {
        EpochManager::enter();  // Enter epoch before reading
        Node* old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next)) {
            old_head = head.load();
        }

        if (old_head) {
            T result = old_head->data;
            EpochManager::retire(old_head);  // Queue for deletion
            EpochManager::exit();  // Exit epoch
            return std::make_shared<T>(result);
        }
        EpochManager::exit();
        return nullptr;
    }
};
```

### Read-Copy-Update (RCU)

**Use Cases:**
- Read-mostly data structures (routing tables, configs)
- Linux kernel data structures
- DNS server implementations
- Network device drivers

**Pros:**
- Zero-cost for readers (no atomic ops)
- Excellent scalability for reads
- Writers can proceed without blocking readers
- Proven in production (Linux kernel)

**Cons:**
- Writes are expensive (copy entire structure)
- Not suitable for write-heavy workloads
- Requires grace period management
- Complex memory reclamation
- Limited to read-mostly scenarios

**Warning:** The simplified RCU example below has a starvation issue - the writer busy-waits for readers count to reach zero, which may never happen if readers keep arriving. Real RCU implementations (like Linux kernel) use grace periods, quiescent states, and deferred reclamation to avoid starvation. This example is for illustration only.

**Example: Complete RCU Implementation with Copy:**
```cpp
template<typename T>
class RCUProtected {
    std::atomic<T*> data_;
    std::atomic<int> readers_ = 0;
    std::vector<T*> to_free_;
    std::mutex mtx_;

public:
    RCUProtected(T* initial) : data_(initial) {}

    ~RCUProtected() {
        delete data_.load();
        for (T* ptr : to_free_) {
            delete ptr;
        }
    }

    // Read-side: zero-cost, no locks
    T* read() {
        readers_.fetch_add(1, std::memory_order_acquire);
        T* result = data_.load(std::memory_order_acquire);
        // Access result...
        readers_.fetch_sub(1, std::memory_order_release);
        return result;
    }

    // Write-side: copy-modify-swap
    void update(std::function<void(T&)> modifier) {
        // 1. Read current data
        T* old = data_.load(std::memory_order_acquire);

        // 2. COPY the entire structure
        T* new_copy = new T(*old);

        // 3. Modify the copy
        modifier(*new_copy);

        // 4. Wait for readers (starvation risk in simplified version)
        while (readers_.load(std::memory_order_acquire) > 0) {
            std::this_thread::yield();
        }

        // 5. Swap pointers atomically
        data_.store(new_copy, std::memory_order_release);

        // 6. Schedule old for deletion
        std::lock_guard<std::mutex> lock(mtx_);
        to_free_.push_back(old);
    }

    void reclaim() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (T* ptr : to_free_) {
            delete ptr;
        }
        to_free_.clear();
    }
};

// Usage:
RCUProtected<std::vector<int>> vec(new std::vector<int>{1, 2, 3});

// Reader (zero-cost):
auto data = vec.read();

// Writer (expensive - copies entire vector):
vec.update([](std::vector<int>& v) {
    v.push_back(4);  // Modify the copy
});
```

### Progress Guarantees

**Use Cases:**
- Designing concurrent algorithms
- Choosing appropriate synchronization primitives
- Real-time systems with latency requirements

**Lock-Free:**
- **Pros:** At least one thread always makes progress
- **Cons:** Individual threads may starve

**Wait-Free:**
- **Pros:** Every thread completes in bounded time
- **Cons:** Often requires more complex algorithms

**Obstruction-Free:**
- **Pros:** Simpler to implement than wait-free
- **Cons:** No progress guarantee under contention

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

**Use Cases:**
- Reducing contention in CAS loops
- High-throughput lock-free algorithms
- Systems with many competing threads

**Exponential Backoff:**
- **Pros:** Adapts to contention level
- **Cons:** Can increase latency

**Fixed Backoff:**
- **Pros:** Predictable behavior
- **Cons:** Doesn't adapt to contention

**Adaptive Backoff:**
- **Pros:** Optimizes for current conditions
- **Cons:** More complex implementation

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

**What is False Sharing:**
False sharing occurs when threads on different cores contend for the same cache line, even though they're accessing different variables. CPU caches work in cache lines (typically 64 bytes), so multiple variables can share the same line. When one thread writes, the entire line is invalidated, causing other threads to reload unnecessarily.

**Without Padding:**
```
Cache Line (64 bytes):
[counter1][counter2][other data...]
Thread A writes counter1 → entire line invalidated
Thread B's counter2 cache also invalidated → cache miss
Result: Cache line bounces between cores, 10-100x slowdown
```

**With Padding:**
```
Cache Line 1 (64 bytes):
[counter1][padding to fill line...]
Thread A writes counter1 → only this line invalidated

Cache Line 2 (64 bytes):
[counter2][padding to fill line...]
Thread B writes counter2 → only this line invalidated
Result: No cache line bouncing, full performance
```

**Use Cases:**
- High-frequency counters accessed by multiple threads
- Sharded data structures
- Performance-critical atomic variables

**Pros:**
- Eliminates false sharing
- Significant performance improvement on multi-core
- Simple to implement

**Cons:**
- Increases memory usage (64 bytes per variable)
- May reduce cache efficiency
- Cache line size varies by architecture

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
