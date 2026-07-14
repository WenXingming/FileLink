# 数据库迁移与 Schema 设计

在 FileLink 项目中，**数据库表结构的变更和版本控制是通过轻量级迁移管理工具 [dbmate](https://github.com/amacneil/dbmate) 实现**的。这确保了所有开发人员和生产环境的数据库结构能够保持高度一致，并具备安全回滚的能力。

本篇文档将介绍 Dbmate 的定义与作用原理、迁移与真实数据的自适应机制、容器化配置，以及数据库迁移的详细执行流程。

---

## 什么是 Dbmate 与“数据库迁移”

为了让团队成员清晰理解数据库迁移的定义，我们需要区分“表结构变更”与“数据搬迁”这两个概念。Dbmate 是一个数据库版本控制工具。它并不负责将一个数据库的数据备份或拷贝到另一个数据库中，它的核心职责是管理数据库的 Schema（ 表结构、索引、约束等）的演进历史。**通过将每一次结构变更（如新建表、添加字段、修改索引）写成包含时间戳的 SQL 脚本并提交到代码仓库，Dbmate 可以自动记录并安全地执行那些尚未在当前数据库运行过的变更**，使数据库结构与代码版本时刻保持同步。

### 迁移时数据库中的真实数据会怎样

在软件工程中，“数据库迁移”（Database Migration）在大多数情况下特指“表结构迁移（Schema Migration）”。**执行迁移时，数据库中现有的用户数据、文件记录等不会被清除或覆盖，它们会在原位保留**。迁移只是**像升级软件一样，在现有数据旁新增表或在现有表中添加新列**。

### 实际数据如何适应新结构（就地转换）

如果表结构发生了破坏性变更（例如需要把原有的 `name` 字段拆分为 `first_name` 和 `last_name`），我们需要**在迁移脚本中写下对应的 SQL**，让数据库在升级结构的同时转换实际数据。这个过程完全是在数据库内部就地完成的，不需要将数据搬出数据库：

```sql
-- 举例：在迁移脚本中就地转换并迁移数据
-- 1. 创建新字段（结构变更）
ALTER TABLE users ADD COLUMN first_name VARCHAR(32), ADD COLUMN last_name VARCHAR(32);

-- 2. 转换并填充老数据（实际数据迁移！）
UPDATE users SET 
    first_name = SUBSTRING(name, 1, 1),
    last_name = SUBSTRING(name, 2);

-- 3. 废弃并删除旧字段（结构变更）
ALTER TABLE users DROP COLUMN name;
```

---

## 迁移文件运作机制

所有的数据库变更脚本都存放在 [database/migrations/](file:///home/wxm/FileLink/database/migrations/) 目录下。迁移脚本的文件名以时间戳作为前缀（如 `202607130001_initial_schema.sql`），以此确保迁移按照正确的历史顺序执行。

每个迁移脚本都被划分为两个方向的生命周期，由特定的 dbmate 注释标签标识：

* **`-- migrate:up`**：代表“向上迁移”。当运行数据库升级时，会执行这部分 SQL 语句来创建表、新增字段或建立索引。
* **`-- migrate:down`**：代表“向下回滚”。当需要撤销该版本变更时，会执行这部分 SQL，通常以相反的依赖顺序对表或结构进行 `DROP` 操作。

在标签中声明的 `transaction:false` 指示 dbmate 不将这些 DDL 语句隐式包装在单个数据库事务中，这在执行某些不允许在事务中运行的 MySQL DDL（例如带有特定外键约束的多表删除与创建）时非常有用。

---

## 核心 Schema 结构设计

初始 Schema 定义在 [202607130001_initial_schema.sql](file:///home/wxm/FileLink/database/migrations/202607130001_initial_schema.sql) 中，它确立了 FileLink 业务模型的元数据事实来源。核心表结构设计如下：

### 账户与认证

* **`users`**：用户账户表，使用 `BINARY(16)`（对应 UUID 二进制字节）作为主键，保存唯一的用户名和 Argon2id 加密后的密码哈希，并使用 `disabled_at` 字段支持软禁用。
* **`user_sessions`**：登录会话表，主键为 `BINARY(32)` 的 `token_hash`。客户端浏览器仅持有原始的随机 Token，数据库只记录其不可逆哈希，从而极大防范了数据库泄露带来的会话劫持风险。

### 文件逻辑与物理分离

* **`objects`**：物理字节对象表，以文件的 BLAKE3 哈希值作为主键。该表独立于任何用户，只记录磁盘文件的实际元数据（如大小）与引用计数（`ref_count`）。这构成了系统跨用户内容去重和秒传的基石。
* **`files`**：逻辑文件表，这是用户视角看到的文件实体。它拥有属于特定用户的 `owner_user_id`，并通过 `content_hash` 指向 `objects` 表。多个用户的逻辑文件可以引用同一个物理对象。

### 共享与分片上传

* **`shares`**：外链分享表，记录了公开分享口令的哈希值、关联的逻辑文件以及失效时间，支持生成限时公开访问的下载链接。
* **`upload_sessions`**：断点续传表，追踪分片上传任务的偏移量进度。它包含严格 of `state` 约束（如 `UPLOADING`、`FINALIZING`、`COMPLETED`），并在状态转换时进行一致性检查。

---

## Dbmate 容器化配置

在本地开发环境中，所有的迁移命令都不需要本地安装 dbmate，而是通过 [docker-compose.yml](file:///home/wxm/FileLink/docker-compose.yml) 中配置的 `migrate` 服务镜像来代理运行。

```yaml
migrate:
  image: ghcr.io/amacneil/dbmate:2.33.0     # 采用官方 Docker 镜像，开箱即用
  restart: "no"                             # 一次性任务，不需要重启
  depends_on:
    mysql:
      condition: service_healthy            # 必须等待 MySQL 容器健康检查通过后再启动
  environment:
    # 传递 MySQL 的网络连接信息与密码
    DATABASE_URL: "mysql://filelink:${FILELINK_MYSQL_PASSWORD}@mysql:3306/filelink"
    DBMATE_MIGRATIONS_DIR: /database/migrations
    DBMATE_NO_DUMP_SCHEMA: "true"
  volumes:
    # 将宿主机的 SQL 变更目录以只读方式挂载到容器中
    - ./database/migrations:/database/migrations:ro
  command: ["--wait", "migrate"]            # 启动命令：等待数据库响应并自动执行迁移
```

---

## 迁移执行流程详解

当我们在终端运行以下应用迁移的指令时，Dbmate 与 MySQL 内部会按如下流水线过程精密配合执行：

```bash
docker compose run --rm migrate
```

### 第一步：容器通信与安全等待

Dbmate 容器随之启动，并基于容器网络通过 TCP 协议指向 MySQL 容器的 3306 端口。因为设置了 `--wait` 参数，如果 MySQL 还在启动过程中，Dbmate 会在循环中安全排队重试，直至数据库开始接受 SQL 连接，以防启动过早而报错中断。

### 第二步：版本记录表校验

建立连接后，Dbmate 会在 MySQL 数据库中检测是否存在名为 `schema_migrations` 的特殊版本记录表。如果不存在（例如在新空库中），它会先隐式执行 SQL 创建该表，主键为 `version`：

```sql
CREATE TABLE schema_migrations (
    version VARCHAR(255) PRIMARY KEY
);
```

### 第三步：查询已应用版本

Dbmate 在 MySQL 内部执行一条普通查询，获取当前数据库已成功运行的所有迁移版本：

```sql
SELECT version FROM schema_migrations;
```

### 第四步：本地文件对比与执行

Dbmate 扫描挂载进来的 [database/migrations/](file:///home/wxm/FileLink/database/migrations/) 目录，读取文件名中的时间戳（如 `202607130001`）。如果该时间戳版本号在第三步中查询出的记录里不存在，Dbmate 会将该 SQL 脚本中 `-- migrate:up` 之后的所有语句发送给 MySQL。MySQL 执行这些 DDL 结构修改和数据转换更新。

### 第五步：打上执行成功标签

如果 MySQL 顺利执行完该版本的所有语句且没有报错，Dbmate 会向 `schema_migrations` 表插入一行记录：

```sql
INSERT INTO schema_migrations (version) VALUES ('202607130001');
```

自此，该版本正式被标记为“已迁移”。在未来的任何一次迁移运行中，它都会被自动忽略。

### 第六步：容器销毁释放

所有新脚本处理完毕后，Dbmate 主进程以状态码 0 正常退出。因为启动时附加了 `--rm`，Docker 守护进程会干净地销毁临时容器，不残留多余的临时环境。

---

## 常用迁移管理指令

* **执行升级（向数据库应用所有未运行的迁移）**：
  ```bash
  docker compose run --rm migrate
  ```
* **查看迁移状态（列出已应用与待应用的文件）**：
  ```bash
  docker compose run --rm migrate status
  ```
* **回滚上一次迁移（反向运行最近一个文件的 down 段并清除版本标记）**：
  ```bash
  docker compose run --rm migrate rollback
  ```

---

## 变更开发建议

当因业务需求需要修改数据库 Schema 时，请遵循以下开发步骤：

* **创建新的迁移文件**：在 [database/migrations/](file:///home/wxm/FileLink/database/migrations/) 目录下，以当前时间戳和简短描述命名，创建新的 `.sql` 文件。
* **编写幂等的 DDL**：在 `-- migrate:up` 中编写结构变更 SQL。在 `-- migrate:down` 中编写完全对应的逆向 SQL。
* **按反向依赖顺序删除**：在回滚段（down）删除表时，应先删除外键依赖表（如 `upload_sessions` 和 `shares`），最后删除被依赖的主表（如 `files` 和 `users`），防止因外键约束冲突导致回滚失败。
