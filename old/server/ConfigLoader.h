#pragma once

#include <string>

#include "FileLinkServerConfig.h"

// 加载 serverRoot/conf/server.conf 并填充配置。
// - 提供 "-r <serverRoot>" 或 "--root <serverRoot>" 时使用该目录。
// - 否则，如果 argv[1] 不是选项，则将 argv[1] 视为 serverRoot。
// - Else: searches a few default roots.
// Returns true on success; on failure returns false and sets outError.
bool load_filelink_server_config(int argc, char* argv[], FileLinkServerConfig& out, std::string& outError);
