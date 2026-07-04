#pragma once

#include "MySqlConfig.h"
#include "MySqlConnection.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>

namespace filelink {

using ConnectionPtr = std::unique_ptr<MySqlConnection, std::function<void(MySqlConnection*)>>;

class MySqlPool {
public:
    explicit MySqlPool(const MySqlConfig& config);
    ~MySqlPool() = default;

    MySqlPool(const MySqlPool&) = delete;
    MySqlPool& operator=(const MySqlPool&) = delete;

    ConnectionPtr acquire();

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<MySqlConnection>> connections_;
};

} // namespace filelink
