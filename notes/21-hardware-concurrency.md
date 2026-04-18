# Hardware Concurrency: Usage and Implementation

## Practical Usage

### std::thread::hardware_concurrency

```cpp
#include <thread>
#include <iostream>

int main() {
    unsigned int cores = std::thread::hardware_concurrency();
    std::cout << "Hardware concurrency: " << cores << "\n";
    std::cout << "Number of concurrent threads supported: " << cores << "\n";
    return 0;
}
```

### Adaptive Thread Pool Size

```cpp
#include <thread>
#include <vector>

class AdaptiveThreadPool {
private:
    std::vector<std::thread> workers_;
    size_t num_threads_;
    
public:
    AdaptiveThreadPool() {
        num_threads_ = std::thread::hardware_concurrency();
        if (num_threads_ == 0) {
            num_threads_ = 4;  // Fallback
        }
        
        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.emplace_back([this] {
                // Worker loop
            });
        }
    }
    
    size_t thread_count() const {
        return num_threads_;
    }
};
```

### CPU Affinity (Linux)

```cpp
#include <thread>
#include <pthread.h>
#include <iostream>

void set_cpu_affinity(std::thread& t, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    
    pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
}

void worker(int id) {
    std::cout << "Worker " << id << " running\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {
    unsigned int cores = std::thread::hardware_concurrency();
    std::cout << "Detected " << cores << " cores\n";
    
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < cores; ++i) {
        threads.emplace_back(worker, i);
        set_cpu_affinity(threads.back(), i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### CPU Affinity (Windows)

```cpp
#include <thread>
#include <windows.h>
#include <iostream>

void set_cpu_affinity(std::thread& t, DWORD_PTR mask) {
    SetThreadAffinityMask(t.native_handle(), mask);
}

void worker(int id) {
    std::cout << "Worker " << id << " running\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {
    unsigned int cores = std::thread::hardware_concurrency();
    std::cout << "Detected " << cores << " cores\n";
    
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < cores; ++i) {
        threads.emplace_back(worker, i);
        set_cpu_affinity(threads.back(), 1ULL << i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

### NUMA-Aware Allocation

```cpp
#include <vector>
#include <thread>

// Linux NUMA support (requires libnuma)
#ifdef __linux__
#include <numa.h>

class NumaAwareAllocator {
public:
    void* allocate(size_t size, int node) {
        return numa_alloc_onnode(size, node);
    }
    
    void deallocate(void* ptr, size_t size) {
        numa_free(ptr, size);
    }
    
    static int current_node() {
        return numa_node_of_cpu(sched_getcpu());
    }
};
#endif

// Windows NUMA support
#ifdef _WIN32
#include <windows.h>

class NumaAwareAllocator {
public:
    void* allocate(size_t size, int node) {
        return VirtualAllocExNuma(GetCurrentProcess(), nullptr, size,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, node);
    }
    
    void deallocate(void* ptr, size_t size) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
    
    static int current_node() {
        PROCESSOR_NUMBER proc_number;
        GetCurrentProcessorNumberEx(&proc_number);
        return proc_number.Group;
    }
};
#endif
```

### Realistic Example: Parallel Processing with Hardware Concurrency

```cpp
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>

template<typename It, typename F>
void parallel_for(It first, It last, F func) {
    auto size = std::distance(first, last);
    unsigned int cores = std::thread::hardware_concurrency();
    
    if (cores == 0) cores = 4;
    
    size_t chunk_size = (size + cores - 1) / cores;
    
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < cores; ++i) {
        auto chunk_start = first + i * chunk_size;
        auto chunk_end = std::min(chunk_start + chunk_size, last);
        
        if (chunk_start >= last) break;
        
        threads.emplace_back([chunk_start, chunk_end, func]() {
            for (auto it = chunk_start; it != chunk_end; ++it) {
                func(*it);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
}

int main() {
    std::vector<int> data(10000);
    std::iota(data.begin(), data.end(), 0);
    
    parallel_for(data.begin(), data.end(), [](int& x) {
        x *= 2;
    });
    
    return 0;
}
```

## Underlying Implementation

### hardware_concurrency Implementation

```cpp
namespace std {
    unsigned int thread::hardware_concurrency() noexcept {
#if defined(_WIN32) || defined(_WIN64)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
#elif defined(__linux__) || defined(__APPLE__)
        long cores = sysconf(_SC_NPROCESSORS_ONLN);
        return cores > 0 ? static_cast<unsigned int>(cores) : 0;
#elif defined(__FreeBSD__)
        int cores;
        size_t len = sizeof(cores);
        sysctlbyname("hw.ncpu", &cores, &len, NULL, 0);
        return cores;
#else
        return 0;  // Unknown
#endif
    }
}
```

### CPU Topology Detection

```cpp
class CpuTopology {
private:
    struct CoreInfo {
        int physical_id;
        int logical_id;
        int numa_node;
    };
    
    std::vector<CoreInfo> cores_;
    
public:
    CpuTopology() {
        detect_topology();
    }
    
    void detect_topology() {
#ifdef __linux__
        // Read /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("physical id") != std::string::npos) {
                // Parse physical ID
            } else if (line.find("processor") != std::string::npos) {
                // Parse logical ID
            }
        }
#elif defined(_WIN32)
        // Use GetLogicalProcessorInformation
        DWORD length = 0;
        GetLogicalProcessorInformation(nullptr, &length);
        
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        GetLogicalProcessorInformation(buffer.data(), &length);
        
        for (const auto& info : buffer) {
            if (info.Relationship == RelationProcessorCore) {
                // Parse core information
            }
        }
#endif
    }
    
    size_t physical_cores() const {
        return cores_.size();
    }
    
    size_t logical_threads() const {
        return cores_.size();
    }
    
    int numa_nodes() const {
#ifdef __linux__
        return numa_max_node() + 1;
#elif defined(_WIN32)
        return GetNumaHighestNodeNumber() + 1;
#else
        return 1;
#endif
    }
};
```

### Cache Line Detection

```cpp
class CacheInfo {
public:
    static size_t cache_line_size() {
#if defined(__x86_64__) || defined(_M_X64)
        return 64;  // Typical x86-64 cache line size
#elif defined(__aarch64__)
        return 64;  // Typical ARM64 cache line size
#elif defined(__powerpc64__)
        return 128;  // PowerPC cache line size
#else
        return 64;  // Default assumption
#endif
    }
    
    static size_t l1_cache_size() {
#if defined(__linux__)
        // Read from /sys/devices/system/cpu/cpu0/cache/index0/size
        // or use sysconf
        return sysconf(_SC_LEVEL1_DCACHE_LINESIZE) * 
               sysconf(_SC_LEVEL1_DCACHE_ASSOC);
#else
        return 32 * 1024;  // 32KB typical
#endif
    }
    
    static size_t l2_cache_size() {
#if defined(__linux__)
        // Read from /sys/devices/system/cpu/cpu0/cache/index2/size
        return 256 * 1024;  // 256KB typical
#else
        return 256 * 1024;
#endif
    }
    
    static size_t l3_cache_size() {
#if defined(__linux__)
        // Read from /sys/devices/system/cpu/cpu0/cache/index3/size
        return 8 * 1024 * 1024;  // 8MB typical
#else
        return 8 * 1024 * 1024;
#endif
    }
};
```

### Hyperthreading Detection

```cpp
class HyperthreadingDetector {
public:
    static bool has_hyperthreading() {
#if defined(_WIN32)
        // Check if logical cores > physical cores
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = nullptr;
        DWORD length = 0;
        
        GetLogicalProcessorInformation(nullptr, &length);
        buffer = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(length);
        GetLogicalProcessorInformation(buffer, &length);
        
        DWORD logical_cores = 0;
        DWORD physical_cores = 0;
        
        for (DWORD i = 0; i < length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
            if (buffer[i].Relationship == RelationProcessorCore) {
                physical_cores++;
                logical_cores += buffer[i].ProcessorGroupCount;
            }
        }
        
        free(buffer);
        return logical_cores > physical_cores;
#elif defined(__linux__)
        // Check /proc/cpuinfo for "siblings" vs "cpu cores"
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        int siblings = 0, cpu_cores = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("siblings") != std::string::npos) {
                siblings = std::stoi(line.substr(line.find(":") + 1));
            } else if (line.find("cpu cores") != std::string::npos) {
                cpu_cores = std::stoi(line.substr(line.find(":") + 1));
            }
        }
        
        return siblings > cpu_cores;
#else
        return false;  // Unknown
#endif
    }
    
    static int threads_per_core() {
        if (!has_hyperthreading()) return 1;
        return 2;  // Typical hyperthreading factor
    }
};
```

### CPU Frequency Detection

```cpp
class CpuFrequency {
public:
    static double current_frequency_mhz() {
#if defined(__linux__)
        // Read from /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq
        std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        double khz;
        freq_file >> khz;
        return khz / 1000.0;
#elif defined(_WIN32)
        // Use QueryPerformanceCounter or WMI
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        return frequency.QuadPart / 1000000.0;
#else
        return 0.0;  // Unknown
#endif
    }
    
    static double max_frequency_mhz() {
#if defined(__linux__)
        // Read from /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq
        std::ifstream freq_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        double khz;
        freq_file >> khz;
        return khz / 1000.0;
#elif defined(_WIN32)
        // Use WMI or registry
        return 0.0;
#else
        return 0.0;
#endif
    }
};
```

### Memory Bandwidth Estimation

```cpp
class MemoryBandwidth {
public:
    static double estimated_bandwidth_gb_s() {
        // Estimate based on memory type and frequency
        // This is a rough estimation
        
#if defined(__linux__)
        // Read memory type from dmidecode or /proc/meminfo
        // For DDR4-3200: ~25.6 GB/s per channel
        // For DDR5-4800: ~38.4 GB/s per channel
        
        // Assume dual channel
        return 51.2;  // GB/s for DDR4-3200 dual channel
#else
        return 51.2;  // Default assumption
#endif
    }
};
```

### NUMA Node Information

```cpp
class NumaInfo {
public:
    static int current_numa_node() {
#if defined(__linux__)
        return numa_node_of_cpu(sched_getcpu());
#elif defined(_WIN32)
        PROCESSOR_NUMBER proc_number;
        GetCurrentProcessorNumberEx(&proc_number);
        return proc_number.Group;
#else
        return 0;
#endif
    }
    
    static std::vector<int> get_numa_nodes() {
        std::vector<int> nodes;
#if defined(__linux__)
        int max_node = numa_max_node();
        for (int i = 0; i <= max_node; ++i) {
            nodes.push_back(i);
        }
#elif defined(_WIN32)
        int max_node = GetNumaHighestNodeNumber();
        for (int i = 0; i <= max_node; ++i) {
            nodes.push_back(i);
        }
#endif
        return nodes;
    }
    
    static size_t node_memory_size(int node) {
#if defined(__linux__)
        long long node_size = numa_node_size64(node, nullptr);
        return node_size;
#elif defined(_WIN32)
        ULONGLONG available_memory;
        GetNumaAvailableMemoryNode(node, &available_memory);
        return available_memory;
#else
        return 0;
#endif
    }
};
```

### Thread-to-Core Mapping

```cpp
class ThreadMapper {
private:
    CpuTopology topology_;
    
public:
    ThreadMapper() : topology_() {}
    
    int map_thread_to_core(size_t thread_id) {
        // Simple round-robin mapping
        return thread_id % topology_.physical_cores();
    }
    
    int map_thread_to_numa_node(size_t thread_id) {
        // Map to NUMA node based on core
        int core = map_thread_to_core(thread_id);
        return topology_.core_to_numa_node(core);
    }
};
```

## Best Practices

1. Use hardware_concurrency to determine optimal thread count
2. Consider hyperthreading when sizing thread pools
3. Use CPU affinity for performance-critical threads
4. Be aware of NUMA topology for memory-intensive workloads
5. Align data structures to cache line size
6. Consider memory bandwidth limitations
7. Profile on target hardware for optimal configuration
8. Handle cases where hardware_concurrency returns 0
9. Use topology-aware scheduling for NUMA systems
10. Monitor CPU frequency scaling effects
