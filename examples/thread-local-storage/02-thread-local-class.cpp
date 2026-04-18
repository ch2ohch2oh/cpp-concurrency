#include <thread>
#include <iostream>
#include <sstream>
#include "../common.h"

class ThreadLocalLogger {
private:
    thread_local static int instance_count_;

public:
    ThreadLocalLogger() {
        instance_count_++;
        std::ostringstream oss;
        oss << "Logger created, instance count: " << instance_count_ << "\n";
        print(oss.str());
    }

    void log(const std::string& message) {
        std::ostringstream oss;
        oss << "[Thread " << std::this_thread::get_id() << "] " << message << "\n";
        print(oss.str());
    }
};

thread_local int ThreadLocalLogger::instance_count_ = 0;

void worker() {
    ThreadLocalLogger logger;
    logger.log("Doing work");
}

int main() {
    std::thread t1(worker);
    std::thread t2(worker);

    worker();

    t1.join();
    t2.join();

    return 0;
}
