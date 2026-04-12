# C++ Concurrency Topics

A comprehensive todo list of C++ concurrency topics that every C++ programmer should know.

## High Priority Topics

- [x] Learn std::thread basics: creation, joining, detaching, and thread management
- [ ] Master mutex types: std::mutex, std::recursive_mutex, std::timed_mutex, and std::shared_mutex
- [ ] Understand lock wrappers: std::lock_guard, std::unique_lock, std::scoped_lock, and std::shared_lock
- [ ] Learn condition variables: std::condition_variable and std::condition_variable_any for thread synchronization
- [ ] Study futures and promises: std::future, std::promise, std::packaged_task, and std::async
- [ ] Master atomic operations: std::atomic, atomic types, and atomic operations on shared data
- [ ] Understand memory ordering: memory_order_relaxed, memory_order_acquire, memory_order_release, memory_order_acq_rel, memory_order_seq_cst
- [ ] Learn deadlock detection and prevention techniques
- [ ] Understand data races vs race conditions and how to prevent them

## Medium Priority Topics

- [ ] Study thread-local storage using thread_local keyword
- [ ] Learn lock-free programming and lock-free data structures
- [ ] Master C++20 synchronization primitives: std::barrier, std::latch, and std::semaphore
- [ ] Understand parallel algorithms from C++17: std::execution::par and std::execution::par_unseq
- [ ] Study thread pool implementation patterns and best practices
- [ ] Learn task-based concurrency patterns and work-stealing queues
- [ ] Understand C++20 coroutines and their use in asynchronous programming
- [ ] Study std::jthread (C++20) and automatic thread joining with std::stop_token

## Low Priority Topics

- [ ] Learn about std::atomic_ref and atomic operations on non-atomic objects (C++20)
- [ ] Understand wait/notify mechanisms: std::atomic::wait and std::atomic::notify_one/notify_all (C++20)
- [ ] Study executor concepts and the future of parallel execution in C++
- [ ] Learn about hardware concurrency: std::thread::hardware_concurrency and CPU affinity
- [x] Understand exception handling in multithreaded contexts

## Learning Path

Start with the high priority topics to build a strong foundation in C++ concurrency, then progress to medium priority topics for advanced patterns, and finally explore low priority topics for specialized use cases.
