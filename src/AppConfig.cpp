#include "AppConfig.h"

#include "CLI/CLI.hpp"

#include <algorithm>
#include <stdexcept>

namespace filelink {

AppConfig parse_app_config(const std::vector<std::string>& args) {
    AppConfig config;
    CLI::App app{ "FileLink 团队文件分发与存储平台" };

    app.set_config("--config", "config/server.toml", "TOML 配置文件路径", false);
    app.add_option("--address", config.listenAddress, "监听地址");
    app.add_option("--port", config.port, "监听端口")
        ->check(CLI::Range(1, 65535));
    app.add_option("--io-threads", config.ioThreads, "I/O 线程数，0 表示自动选择")
        ->check(CLI::NonNegativeNumber);
    app.add_option("--storage-root", config.storageRoot, "文件存储根目录")
        ->check([](const std::string& value) {
        return value.empty() ? "存储根目录不能为空" : std::string();
            });
    app.add_option("--web-root", config.webRoot, "静态资源根目录");
    app.add_option("--log-root", config.logRoot, "日志文件根目录");
    app.add_flag("--cleanup-expired", config.cleanupExpiredOnly, "仅清理过期的会话与临时文件并退出");
    app.add_flag("--reclaim-pending-objects", config.reclaimPendingObjectsOnly,
        "离线回收零引用对象并退出");
    app.add_flag("--scan-orphaned-objects", config.scanOrphanedObjectsOnly,
        "离线扫描没有 objects 记录的对象文件并退出");
    app.add_flag("--reclaim-orphaned-objects", config.reclaimOrphanedObjectsOnly,
        "离线删除没有 objects 记录的对象文件并退出");

    // MySQL 配置
    app.add_option("--mysql-host", config.mysql.host, "MySQL 主机地址");
    app.add_option("--mysql-port", config.mysql.port, "MySQL 端口")
        ->check(CLI::Range(1, 65535));
    app.add_option("--mysql-user", config.mysql.user, "MySQL 用户名");
    app.add_option("--mysql-database", config.mysql.database, "MySQL 数据库名");
    app.add_option("--mysql-connect-timeout-seconds", config.mysql.connectTimeoutSeconds, "MySQL 连接超时时间(秒)");
    app.add_option("--mysql-pool-size", config.mysql.poolSize, "MySQL 连接池大小");
    
    // 密码只能通过环境变量或命令行传递，禁止写入 TOML 文件
    app.add_option("--mysql-password", config.mysql.password, "MySQL 密码")
        ->envname("FILELINK_MYSQL_PASSWORD");

    // CLI11 的 vector 接口按栈顺序消费参数，对调用方仍暴露自然的命令行顺序。
    std::vector<std::string> parseArgs(args.rbegin(), args.rend());
    app.parse(parseArgs);
    return config;
}

void validate_app_config(const AppConfig& config) {
    if (config.mysql.password.empty()) {
        throw std::invalid_argument(
            "缺少必需配置 FILELINK_MYSQL_PASSWORD，请通过环境变量设置 MySQL 密码");
    }
}

} // namespace filelink
