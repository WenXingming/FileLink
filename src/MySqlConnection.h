#pragma once

#include <cstdint>
#include <string>

struct MYSQL;

#include "MySqlConfig.h"

namespace filelink {

class MySqlConnection {
public:
    explicit MySqlConnection(const MySqlConfig& config);
    ~MySqlConnection();

    MySqlConnection(const MySqlConnection&) = delete;
    MySqlConnection& operator=(const MySqlConnection&) = delete;

    MySqlConnection(MySqlConnection&& other) noexcept;
    MySqlConnection& operator=(MySqlConnection&& other) noexcept;

    // 一个连接同一时刻只能由一个线程使用。
    bool ping();

private:
    MYSQL* connection_ = nullptr;
};

} // namespace filelink
