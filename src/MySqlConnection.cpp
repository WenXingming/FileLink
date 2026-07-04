#include "MySqlConnection.h"

#include <mysql.h>

#include <stdexcept>
#include <utility>

namespace {

class MySqlLibrary {
public:
    MySqlLibrary() {
        if (::mysql_library_init(0, nullptr, nullptr) != 0) {
            throw std::runtime_error("MySQL 客户端库初始化失败");
        }
    }

    ~MySqlLibrary() {
        ::mysql_library_end();
    }
};

void ensure_mysql_library_initialized() {
    static const MySqlLibrary library;
    (void)library;
}

[[noreturn]] void close_and_throw(MYSQL* connection, const std::string& action) {
    const std::string message = action + ": " + ::mysql_error(connection);
    ::mysql_close(connection);
    throw std::runtime_error(message);
}

} // namespace

namespace filelink {

MySqlConnection::MySqlConnection(const MySqlConfig& config) {
    if (config.host.empty() || config.user.empty() || config.database.empty()) {
        throw std::invalid_argument("MySQL 主机、用户和数据库不能为空");
    }
    if (config.connectTimeoutSeconds == 0) {
        throw std::invalid_argument("MySQL 连接超时必须大于 0");
    }

    ensure_mysql_library_initialized();
    MYSQL* connection = ::mysql_init(nullptr);
    if (connection == nullptr) {
        throw std::runtime_error("MySQL 连接句柄初始化失败");
    }

    if (::mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &config.connectTimeoutSeconds) != 0) {
        close_and_throw(connection, "设置 MySQL 连接超时失败");
    }
    if (::mysql_options(connection, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        close_and_throw(connection, "设置 MySQL 字符集失败");
    }

    if (mysql_real_connect(connection,
        config.host.c_str(),
        config.user.c_str(),
        config.password.empty() ? nullptr : config.password.c_str(),
        config.database.c_str(),
        config.port,
        nullptr,
        0) == nullptr) {
        close_and_throw(connection, "连接 MySQL 失败");
    }
    connection_ = connection;
}

MySqlConnection::~MySqlConnection() {
    if (connection_ != nullptr) {
        ::mysql_close(connection_);
    }
}

MySqlConnection::MySqlConnection(MySqlConnection&& other) noexcept
    : connection_(std::exchange(other.connection_, nullptr)) {}

MySqlConnection& MySqlConnection::operator=(MySqlConnection&& other) noexcept {
    if (this != &other) {
        if (connection_ != nullptr) {
            ::mysql_close(connection_);
        }
        connection_ = std::exchange(other.connection_, nullptr);
    }
    return *this;
}

bool MySqlConnection::ping() {
    return connection_ != nullptr && ::mysql_ping(connection_) == 0;
}

} // namespace filelink
