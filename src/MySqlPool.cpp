#include "MySqlPool.h"

#include <stdexcept>
#include <utility>

namespace filelink {

MySqlPool::MySqlPool(const MySqlConfig& config) {
    if (config.poolSize == 0) {
        throw std::invalid_argument("MySQL 连接池大小不能为 0");
    }

    for (unsigned int i = 0; i < config.poolSize; ++i) {
        connections_.push(std::make_unique<MySqlConnection>(config));
    }
}

ConnectionPtr MySqlPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !connections_.empty(); });

    auto conn = std::move(connections_.front());
    connections_.pop();

    return ConnectionPtr(conn.release(), [this](MySqlConnection* c) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(std::unique_ptr<MySqlConnection>(c));
        cv_.notify_one();
    });
}

} // namespace filelink
