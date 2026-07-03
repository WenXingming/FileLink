#include "AppConfig.h"

#include "CLI/CLI.hpp"

#include <algorithm>

namespace filelink {

AppConfig parse_app_config(const std::vector<std::string>& args) {
    AppConfig config;
    CLI::App app{ "FileLink 团队文件分发与存储平台" };

    app.set_config("--config", "", "TOML 配置文件路径", false);
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

    // CLI11 的 vector 接口按栈顺序消费参数，对调用方仍暴露自然的命令行顺序。
    std::vector<std::string> parseArgs(args.rbegin(), args.rend());
    app.parse(parseArgs);
    return config;
}

} // namespace filelink
