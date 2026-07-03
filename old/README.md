# FileLink 旧实现

> 本目录仅用于查阅重写前的行为和设计，不参与当前构建。

FileLink 是基于 [Tudou](https://github.com/WenXingming/Tudou) 的团队文件分发与存储服务。目前支持文件上传、下载链接生成、基于 SHA-256 的内容寻址存储，以及可选的鉴权、MySQL 元数据持久化、Redis 热点缓存和 HTTPS。

<p align="center">
  <img src="./assets/filelink-server.png" alt="filelink-server" width="88%" />
</p>

<p align="center">
  <img src="./assets/filelink-server-mysql.png" alt="filelink-server mysql" width="44%" />
  <img src="./assets/filelink-server-redis.png" alt="filelink-server redis" width="44%" />
</p>

## 目录结构

```text
FileLink/
├── src/                 # 在线服务实现与可复用核心模块
│   ├── dedup/           # 离线重复文件发现引擎
│   ├── auth/            # 登录鉴权
│   ├── filestore/       # 文件系统存储
│   ├── metastore/       # 元数据持久化
│   └── metacache/       # 元数据缓存
├── tools/dedup/         # dedup 命令行入口
├── tests/dedup/         # dedup 单元测试与 CLI 测试
├── benchmarks/dedup/    # dedup 性能基准
├── docs/dedup/          # dedup 设计与评测文档
├── config/
│   ├── filelink/        # FileLink 配置、页面与运行目录
│   └── nginx/           # Nginx 反向代理配置
└── assets/              # README 展示资源
```

在线服务与离线治理共享同一仓库，但保持独立 target：`filelink-server` 不依赖 dedup CLI，`FileLink::dedup` 也不依赖 HTTP 服务。

## 构建

FileLink 会优先查找本机安装的 Tudou；未找到时，通过 CMake `FetchContent` 自动获取源码。

```bash
cmake -S . -B build
cmake --build build -j
```

在同时开发本地 Tudou 时，可以覆盖下载源：

```bash
cmake -S . -B build \
  -DFETCHCONTENT_SOURCE_DIR_TUDOU=/home/wxm/Tudou
cmake --build build -j
```

MySQL Connector/C++ 和 hiredis 均为可选依赖。缺少它们时，项目仍可构建，并分别退化为内存元数据存储和空缓存实现。

dedup 工具默认参与构建：

```bash
./build/tools/dedup/filelink-dedup /path/to/history
```

构建并运行 dedup 测试：

```bash
cmake -S . -B build-test -DFILELINK_BUILD_TESTS=ON
cmake --build build-test -j
ctest --test-dir build-test --output-on-failure
```

当前迁入的是重复文件扫描与报告能力；涉及 FileLink 元数据事务、对象引用计数和垃圾回收的业务适配将在后续实现。

## 开发依赖

仓库通过 Docker Compose 提供 MySQL 和 Redis，FileLink 服务本身仍在宿主机运行：

```bash
cp .env.example .env
docker compose up -d
```

如需启用它们，请同步修改 `config/filelink/conf/server.conf` 中的 MySQL 密码，并将 `mysql.enabled`、`redis.enabled` 设为 `true`。数据保存在 Docker named volumes 中；执行 `docker compose down` 不会删除数据。

Nginx 已作为可选的 `proxy` profile 声明，默认不会启动。先启动 FileLink，再按需启用反向代理：

```bash
docker compose --profile proxy up -d nginx
```

Nginx 使用 host 网络监听宿主机 `80` 端口，并将请求转发到 `127.0.0.1:18081`。

## 运行

仓库提供的默认配置不包含密码，且关闭鉴权、MySQL、Redis 与 HTTPS：

```bash
./build/filelink-server -r ./config/filelink
```

默认访问地址为 `http://127.0.0.1:18081`，主要接口包括：

- `GET /`：访问 Web 首页。
- `POST /login`：登录并获取令牌。
- `POST /upload`：上传文件，请求头通过 `X-File-Name` 指定文件名。
- `GET /file/{id}`：按文件 ID 下载。

运行日志写入 `config/filelink/log/server.log`，上传内容写入 `config/filelink/storage/`；两者均不会提交到版本库。
