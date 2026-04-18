#pragma once

#include <iostream>
#include <mutex>
#include <string>

inline std::mutex cout_mutex;

inline void print(const std::string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << msg;
}
