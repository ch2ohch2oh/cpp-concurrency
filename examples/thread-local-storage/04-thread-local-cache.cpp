#include <thread>
#include <unordered_map>
#include <string>
#include <format>
#include "../common.h"

class ThreadLocalCache {
private:
    thread_local static std::unordered_map<std::string, int> cache_;
    thread_local static int hit_count_;
    thread_local static int miss_count_;

public:
    static int get(const std::string& key) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            hit_count_++;
            return it->second;
        }
        miss_count_++;
        int value = compute_value(key);
        cache_[key] = value;
        return value;
    }

    static void print_stats() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        print(std::format("Thread {} cache stats: {} hits, {} misses\n",
                  oss.str(), hit_count_, miss_count_));
    }

private:
    static int compute_value(const std::string& key) {
        // Expensive computation
        return key.length();
    }
};

thread_local std::unordered_map<std::string, int> ThreadLocalCache::cache_;
thread_local int ThreadLocalCache::hit_count_ = 0;
thread_local int ThreadLocalCache::miss_count_ = 0;

void worker(const std::string& key1, const std::string& key2) {
    // First access - cache miss
    int val1 = ThreadLocalCache::get(key1);
    // Second access to same key - cache hit
    int val2 = ThreadLocalCache::get(key1);
    // Different key - cache miss
    int val3 = ThreadLocalCache::get(key2);
    // Repeat access - cache hit
    int val4 = ThreadLocalCache::get(key2);

    std::ostringstream oss;
    oss << std::this_thread::get_id();
    print(std::format("Thread {}: {}={}, {}={}\n",
              oss.str(), key1, val1, key2, val3));

    ThreadLocalCache::print_stats();
}

int main() {
    std::thread t1(worker, "hello", "world");
    std::thread t2(worker, "foo", "bar");
    std::thread t3(worker, "test", "data");

    worker("hello", "world");

    t1.join();
    t2.join();
    t3.join();

    return 0;
}
