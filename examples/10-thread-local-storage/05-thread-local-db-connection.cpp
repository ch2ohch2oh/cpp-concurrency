#include <thread>
#include <string>
#include <sstream>
#include "../common.h"

class DatabaseConnection {
private:
    std::string connection_string_;

public:
    DatabaseConnection(const std::string& conn_str)
        : connection_string_(conn_str) {
        std::ostringstream oss;
        oss << "DatabaseConnection created for thread "
            << std::this_thread::get_id() << "\n";
        print(oss.str());
        // Expensive connection setup would happen here
    }

    void query(const std::string& sql) {
        std::ostringstream oss;
        oss << "Thread " << std::this_thread::get_id()
            << " executing: " << sql << "\n";
        print(oss.str());
        // Execute query using connection
    }
};

class ConnectionPool {
private:
    thread_local static DatabaseConnection* connection_;

public:
    static DatabaseConnection& get_connection() {
        if (!connection_) {
            connection_ = new DatabaseConnection("localhost:5432");
        }
        return *connection_;
    }

    static void cleanup() {
        delete connection_;
        connection_ = nullptr;
    }
};

thread_local DatabaseConnection* ConnectionPool::connection_ = nullptr;

void worker(int id) {
    auto& conn = ConnectionPool::get_connection();
    conn.query("SELECT * FROM table_" + std::to_string(id));
}

int main() {
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    std::thread t3(worker, 3);

    worker(0);

    t1.join();
    t2.join();
    t3.join();

    ConnectionPool::cleanup();

    return 0;
}
