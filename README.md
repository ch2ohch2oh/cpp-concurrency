# C++ Concurrency Topics

A comprehensive todo list of C++ concurrency topics that every C++ programmer should know.

## High Priority Topics

- [x] Learn `std::thread` basics: creation, joining, detaching, and thread management ([notes](notes/thread.md))
- [x] Master mutex types: `std::mutex`, `std::recursive_mutex`, `std::timed_mutex`, and `std::shared_mutex` ([notes](notes/mutex-types.md))
- [x] Understand lock wrappers: `std::lock_guard`, `std::unique_lock`, `std::scoped_lock`, and `std::shared_lock` ([notes](notes/lock-wrappers.md))
- [x] Learn condition variables: `std::condition_variable` and `std::condition_variable_any` for thread synchronization ([notes](notes/condition-variables.md))
- [x] Study futures and promises: `std::future`, `std::promise`, `std::packaged_task`, and `std::async` ([notes](notes/futures-promises.md))
- [x] Master atomic operations: `std::atomic`, atomic types, and atomic operations on shared data ([notes](notes/atomic-operations.md))
- [x] Understand memory ordering: `memory_order_relaxed`, `memory_order_acquire`, `memory_order_release`, `memory_order_acq_rel`, `memory_order_seq_cst` ([notes](notes/memory-ordering.md))
- [x] Learn deadlock detection and prevention techniques ([notes](notes/deadlock-detection-prevention.md))
- [x] Understand data races vs race conditions and how to prevent them ([notes](notes/data-races-race-conditions.md))

## Medium Priority Topics

- [x] Study thread-local storage using `thread_local` keyword ([notes](notes/thread-local-storage.md))
- [x] Learn lock-free programming and lock-free data structures ([notes](notes/lock-free-programming.md))
- [x] Master C++20 synchronization primitives: `std::barrier`, `std::latch`, and `std::semaphore` ([notes](notes/cpp20-sync-primitives.md))
- [x] Understand parallel algorithms from C++17: `std::execution::par` and `std::execution::par_unseq` ([notes](notes/parallel-algorithms.md))
- [ ] Study thread pool implementation patterns and best practices ([notes](notes/thread-pool.md))
- [ ] Learn task-based concurrency patterns and work-stealing queues ([notes](notes/task-based-concurrency.md))
- [ ] Understand C++20 coroutines and their use in asynchronous programming ([notes](notes/cpp20-coroutines.md))
- [ ] Study `std::jthread` (C++20) and automatic thread joining with `std::stop_token` ([notes](notes/std-jthread.md))

## Low Priority Topics

- [ ] Learn about `std::atomic_ref` and atomic operations on non-atomic objects (C++20) ([notes](notes/atomic-ref.md))
- [ ] Understand wait/notify mechanisms: `std::atomic::wait` and `std::atomic::notify_one/notify_all` (C++20) ([notes](notes/atomic-wait-notify.md))
- [ ] Study executor concepts and the future of parallel execution in C++ ([notes](notes/executor-concepts.md))
- [ ] Learn about hardware concurrency: `std::thread::hardware_concurrency` and CPU affinity ([notes](notes/hardware-concurrency.md))
- [x] Understand exception handling in multithreaded contexts ([notes](notes/exception-handling.md))

## Learning Path

Start with the high priority topics to build a strong foundation in C++ concurrency, then progress to medium priority topics for advanced patterns, and finally explore low priority topics for specialized use cases.
