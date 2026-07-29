# Uploads：取消上传与异步回收设计

**业务只和数据库打交道、Cleaner 负责回收物理磁盘内容，好处一是提升业务响应速度，而是逻辑解耦合**。因此取消上传不是一次“删除磁盘文件”的动作，而是分成两个阶段：在线请求先在数据库中终止上传的业务资格，离线 Cleaner 再回收尚未发布的临时字节。这样，HTTP 请求有清晰且可重试的业务结果，磁盘删除失败也不会丢失回收任务。

本文只讨论尚未完成的 TUS 上传会话。已经完成的 `File` / `Object` 删除属于文件删除和 Object 回收流程，不经过 `DELETE /uploads/<id>`。

## 先记住结论

```text
DELETE /uploads/<id>
    ↓
数据库：UPLOADING → ABORTED
    ↓
客户端立即不能继续 PATCH
    ↓
保留 .part 与 ABORTED Session
    ↓
离线 Cleaner
    ↓
unlink .part 成功或 ENOENT
    ↓
删除 UploadSession
```

因此，`204 No Content` 的含义是“上传在业务上已经取消”，而不是“.part 已在这一刻从磁盘消失”。

## 涉及的数据与状态

一个未完成上传同时有三处状态：数据库中的 `UploadSession`、磁盘上的 `.part` 临时文件，以及进程内的 BLAKE3 哈希器缓存。它们的所有权不同，处理顺序也不同。


| 位置                            | 内容                         | 取消时如何处理                              |
| --------------------------------- | ------------------------------ | --------------------------------------------- |
| `upload_sessions`               | 所有者、偏移、状态、过期时间 | 在线请求原子更新为`ABORTED`，是业务事实来源 |
| `storage/uploads/<id>.part`     | 已收到但未发布的字节         | 不在 HTTP 请求中删除，交由 Cleaner 重试删除 |
| `UploadService::activeHashers_` | 进程内流式 BLAKE3 状态       | 取消成功后立即移除；它不是可靠存储          |

UploadSession 的相关状态机如下：

```text
                    最后一片
UPLOADING ───────────────────► FINALIZING ───► COMPLETED
    │                              │
    │ DELETE                       │ DELETE 被拒绝（409）
    ▼                              │
 ABORTED                           ▼
    │                           FAILED
    │
    └── Cleaner 成功回收后删除记录

UPLOADING 且 expires_at < NOW()
    ▼
 EXPIRED ── Cleaner 成功回收后删除记录
```

`ABORTED` 和 `EXPIRED` 都是逻辑终态。它们在数据库中存在的时间只用于驱动可靠的物理回收；文件删掉后，Cleaner 删除该行记录。

## 在线取消：DELETE /uploads/<id></id>

客户端请求：

```http
DELETE /uploads/3a4e... HTTP/1.1
Tus-Resumable: 1.0.0
Cookie: filelink_session=<token>
```

[UploadApiRouter](../src/uploads/UploadApiRouter.cpp) 依次验证 HTTP 方法、登录用户和 upload ID，然后调用 `UploadService::terminate_session`。路由根据业务结果映射 HTTP 状态：


| 条件                                         | 状态码                      | 含义                                       |
| ---------------------------------------------- | ----------------------------- | -------------------------------------------- |
| 当前用户拥有一个`UPLOADING` 会话，且成功终止 | `204 No Content`            | 逻辑取消成功                               |
| 会话正在`FINALIZING`                         | `409 Conflict`              | 已进入发布阶段，不能再取消                 |
| 会话不存在、不属于当前用户，或已经终止/完成  | `404 Not Found`             | 没有可取消的上传                           |
| 数据库异常                                   | `500 Internal Server Error` | 取消结果未知，客户端可按业务策略重试或查询 |

服务层不采用“先读状态、再无条件更新”的做法，而使用条件更新把判断和状态转换合为一个数据库动作：

```cpp
bool UploadSessionDao::abort_if_uploading(const std::string& upload_id,
    const std::string& owner_user_id) {
    soci::statement statement = (sql_.prepare
        << "UPDATE upload_sessions SET state = 'ABORTED' "
           "WHERE upload_id = :id AND owner_user_id = :owner AND state = 'UPLOADING'",
        soci::use(upload_id),
        soci::use(owner_user_id));
    statement.execute(false);
    return statement.get_affected_rows() == 1;
}
```

这条 SQL 同时保证了所有权和当前状态。影响一行表示本次取消成功；影响零行时服务再读取会话，只有确实是当前用户的 `FINALIZING` 会话才返回 409，其余情况统一隐藏为 404。

`UploadService` 在成功更新数据库后只释放内存状态：

```cpp
db::UploadSessionDao sessionStore(sql);
if (!sessionStore.abort_if_uploading(uploadIdBinary, ownerUserId)) {
    // 区分 FINALIZING（409）与不可取消（404）
    ...
}

{
    std::lock_guard<std::mutex> lock(hashersMutex_);
    activeHashers_.erase(uploadIdHex);
}

return UploadTerminationResult::Terminated;
```

这里没有 `stat`、`unlink` 或 ObjectStore 调用。数据库成功提交后，业务上即使 `.part` 还存在，也已经是无主临时数据，客户端无法通过原上传会话继续使用它。

## PATCH 与取消的边界

每次 PATCH 都在写入前读取 Session，并只接受 `UPLOADING`：

```cpp
if (!sessionStore.find(uploadIdBinary, session) || session.owner_user_id != ownerUserId) {
    return UploadChunkResult::SessionNotFound;
}
if (session.state != "UPLOADING") {
    return UploadChunkResult::SessionNotFound;
}
```

所以取消的数据库状态提交后，后续 PATCH 会被路由映射为 `404 Not Found`。这防止了已取消会话重新进入上传流程。

仍需理解一个正常的并发边界：若 PATCH 在 DELETE 提交前已经读取到 `UPLOADING` 并开始文件写入，它可能完成这一次写入；但 PATCH 不会把 `ABORTED` 更新回 `UPLOADING`。Cleaner 最终会删除这段临时字节。当前设计保证“取消提交后的新 PATCH 被拒绝”，并不承诺“204 返回的瞬间没有任何正在执行的磁盘写入”。如未来需要更强的停止语义，应为 PATCH 和 DELETE 引入每会话互斥或数据库占用状态。

## 离线回收：UploadSessionCleaner

Cleaner 由 [UploadSessionCleaner](../src/cleaner/UploadSessionCleaner.cpp) 实现。它每次运行时先将过期会话标为 `EXPIRED`，再统一扫描 `ABORTED` 和 `EXPIRED`：

```cpp
sql << "UPDATE upload_sessions SET state = 'EXPIRED' "
       "WHERE state = 'UPLOADING' AND expires_at < NOW()";

soci::rowset<std::string> rows = (sql.prepare <<
    "SELECT upload_id FROM upload_sessions WHERE state IN ('ABORTED', 'EXPIRED')");
```

扫描结果先复制到内存列表，再逐个删除对应的 `.part`。复制列表避免在迭代查询结果时修改同一张表：

```cpp
for (const std::string& idBinary : uploadIds) {
    const std::string idHex = bytes_to_hex(idBinary);
    const std::string partPath = storageRoot_ + "/uploads/" + idHex + ".part";

    if (::unlink(partPath.c_str()) != 0 && errno != ENOENT) {
        // 保留 ABORTED / EXPIRED Session，下次运行自动重试。
        continue;
    }

    soci::transaction tr(sql);
    soci::statement statement = (sql.prepare
        << "DELETE FROM upload_sessions "
           "WHERE upload_id = :id AND state IN ('ABORTED', 'EXPIRED')",
        soci::use(idBinary));
    statement.execute(false);
    tr.commit();
}
```

`ENOENT` 被当成成功，因为文件可能已经被人工清理、前一次 Cleaner 删除后进程崩溃，或多个 Cleaner 并发处理。相反，权限、I/O 或目录类型错误会使 `unlink` 失败；Cleaner 记录告警但不删除 Session，从而留下明确、可重试的任务。

完整结果表：


| Cleaner 删除`.part` 的结果          | Session 结果              | 下次行为                        |
| ------------------------------------- | --------------------------- | --------------------------------- |
| 成功                                | 删除 Session              | 不再处理                        |
| `ENOENT`                            | 删除 Session              | 不再处理                        |
| 其他错误                            | 保留`ABORTED` / `EXPIRED` | 下次继续尝试                    |
| 删除文件后、删除 Session 前进程崩溃 | 保留终态 Session          | 下次看到`ENOENT` 后删除 Session |

## 运维调度

Cleaner 是一次性离线命令，不会随 HTTP 服务常驻运行。当前保留的 CLI 名称是 `--cleanup-expired`，但它实际调用 `cleanup_terminated_sessions`，因此同时处理超时和主动取消：

```bash
./scripts/run-dev.sh --cleanup-expired
```

生产环境必须将它交给调度器。下面是 systemd timer 的示意，具体用户、工作目录和环境变量应按部署方式调整：

```ini
# /etc/systemd/system/filelink-cleanup.service
[Service]
Type=oneshot
WorkingDirectory=/srv/filelink
ExecStart=/srv/filelink/scripts/run-dev.sh --cleanup-expired
```

```ini
# /etc/systemd/system/filelink-cleanup.timer
[Timer]
OnBootSec=2min
OnUnitActiveSec=10min
Persistent=true

[Install]
WantedBy=timers.target
```

调度周期决定了取消后临时文件最多保留多久。磁盘容量紧张、上传文件很大时应缩短周期；普通部署可从 5 至 10 分钟开始，根据 Cleaner 日志和剩余磁盘空间调整。

## 测试覆盖

现有集成测试覆盖以下关键场景：

- DELETE 后 Session 变为 `ABORTED`，`.part` 不在请求路径删除；
- 取消成功后 PATCH 返回 404；
- Cleaner 回收主动取消的 `.part` 和 Session；
- 过期 `UPLOADING` 会话被标记并回收；
- `.part` 不存在时，`ENOENT` 仍能完成回收；
- `unlink` 失败时 Session 被保留，下一轮成功后才删除。

相关测试位于 [TusControlApiTest.cpp](../tests/TusControlApiTest.cpp) 和 [UploadSessionDaoTest.cpp](../tests/UploadSessionDaoTest.cpp)。

## 与 Object 回收的区别

上传取消只处理未发布的 `.part`，它没有对应的 `Object` 或 `File`。完成上传后删除用户文件则是另一条流程：数据库先减少 Object 引用并将零引用 Object 标为 `PENDING_DELETE`，`ObjectReclaimer` 再物理删除正式内容对象。两者都采用“数据库先表达业务终态、离线任务后处理物理字节”的原则，但处理的数据模型不同。
