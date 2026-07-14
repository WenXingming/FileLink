# Cleaner：对象回收与离线维护设计

在线请求的职责是快速响应用户操作，保障极低的 HTTP 延迟；而深度磁盘扫描、大文件物理删除以及奔溃一致性补偿则适合剥离为离线后台任务。FileLink 因此将“逻辑删除成功”与“物理存储回收”进行了异步化隔离：用户在客户端删除文件，系统立即在事务中抹去关系记录，而最终的物理磁盘字节释放则由 `cleaner` 模块在后台安全、异步地重试完成。

本篇文档将详细剖析三个独立清理器（过期上传会话、零引用物理对象、磁盘孤儿文件）的业务逻辑、状态机跳转与具体的 C++ 代码实现。

---

## 离线维护设计蓝图

通过将慢速磁盘 I/O（如 `unlink`）从 HTTP 请求中抽离，用户操作只关心元数据是否变更，极大提升了系统在高并发吞吐下的表现。三类遗留数据的清理策略如下表所示：

| 清理器类 | 处理目标 | 激活参数 | 物理操作行为 |
| :--- | :--- | :--- | :--- |
| [UploadSessionCleaner](file:///home/wxm/FileLink/src/cleaner/UploadSessionCleaner.h#L16) | 过期且中断的临时的 `.part` 上传分片文件 | `--cleanup-expired` | 物理删除临时文件，并将数据库状态标记为 `EXPIRED`。 |
| [ObjectReclaimer](file:///home/wxm/FileLink/src/cleaner/ObjectReclaimer.h#L17) | 引用计数归零（`ref_count = 0`）的物理文件 | `--reclaim-pending-objects` | 物理认领待删除文件，`unlink` 成功后物理抹除数据库 `objects` 记录。 |
| [ObjectOrphanReclaimer](file:///home/wxm/FileLink/src/cleaner/ObjectOrphanReclaimer.h#L15) | 磁盘上存在但 MySQL `objects` 表无记录的残留文件 | `--scan-orphaned-objects` / `--reclaim-orphaned-objects` | 双重检索核对，原子清除事务崩溃留下的磁盘残留。 |

这些参数由 `main.cpp` 在启动阶段读取，如果指定了这些 CLI 参数，进程将执行完指定的离线维护任务后立即安全退出，而不会启动 HTTP 服务。这种设计极大地方便了运维使用 cron 守护进程、systemd timer 或 Kubernetes CronJob 按照预设的低峰期策略进行调度。

---

## 零引用物理对象回收状态机

为了保证高并发上传（秒传）与离线物理回收之间不发生竞态冲突，`objects.state` 在生命周期中进行如下状态流转：

```text
       File 删除
 READY ─────────► PENDING_DELETE (待删除)
                       │
                  回收器原子认领
                       ▼
                  RECLAIMING (回收中)
                       │
            ┌──────────┴──────────┐
      unlink 成功             unlink 失败
            ▼                     ▼
     物理抹除 DB 记录         退回 PENDING_DELETE
```

### 1. 回收器的原子认领与物理删除

在 [ObjectReclaimer::reclaim_pending_objects](file:///home/wxm/FileLink/src/cleaner/ObjectReclaimer.cpp#L27) 中，回收器首先查询所有引用计数为 0 且状态为 `PENDING_DELETE` 的记录，并通过乐观锁更新（`claim_pending_delete`）将其原子锁定为 `RECLAIMING`。这能确保在多个清理器进程并发运行时，同一个物理对象仅会被一个进程认领，避免了重复删除与文件描述符损坏。

```cpp
int ObjectReclaimer::reclaim_pending_objects() {
    db::SociSessionLease lease(pool_);
    db::ObjectDao objects(lease.get());
    std::vector<db::Object> candidates;
    // 1. 获取所有待回收的候选列表
    objects.find_reclaimable(candidates);

    int reclaimed_count = 0;
    for (const db::Object& object : candidates) {
        // 2. 乐观锁认领：尝试将状态从 PENDING_DELETE 修改为 RECLAIMING
        if (object.state == "PENDING_DELETE" && !objects.claim_pending_delete(object.content_hash)) {
            continue; // 认领失败（已被其他进程认领），跳过
        }

        const std::string object_path = store_.get_object_path(hex_encode(object.content_hash));
        
        // 3. 执行磁盘物理擦除 (ENOENT 异常在此视为已删除)
        if (::unlink(object_path.c_str()) != 0 && errno != ENOENT) {
            // 物理删除失败（如磁盘 I/O 阻塞或只读），退回 PENDING_DELETE 状态供下一次重试
            objects.return_to_pending_delete(object.content_hash);
            continue;
        }
        
        // 4. 物理文件删除成功后，物理抹除 MySQL 中的 Object 记录
        if (objects.remove_reclaiming(object.content_hash)) {
            ++reclaimed_count;
        }
    }
    return reclaimed_count;
}
```

### 2. 竞态防御：拒绝复活已认领对象

当回收器已将对象标记为 `RECLAIMING` 后，如果前端在此瞬间再次上传了相同内容的文件并尝试执行秒传，在 [ObjectDao::add_reference](file:///home/wxm/FileLink/src/database/Object.cpp#L8) 中，数据库会拦截这一操作：
* `add_reference` 判定如果对象处于 `RECLAIMING` 状态，则不会对其进行 `ref_count + 1`，而是直接返回 `Reclaiming` 失败状态。
* 业务层收到失败后，会将会话标记为 `FAILED`，引导前端转为正常的重新上传，而绝不会在磁盘文件被物理擦除的瞬间，将一个空引用文件绑定给新用户，从底层保障了数据绝对不丢失。

---

## 过期上传会话清理

有些客户端在大文件传输中途直接关闭了浏览器，会导致磁盘上遗留下大量未完成的 `.part` 临时文件。在 [UploadSessionCleaner::cleanup_expired_sessions](file:///home/wxm/FileLink/src/cleaner/UploadSessionCleaner.cpp#L35) 中，系统会找出这些已失效的传输会话进行空间释放：

```cpp
int UploadSessionCleaner::cleanup_expired_sessions() {
    int successCount = 0;
    try {
        db::SociSessionLease lease(pool_);
        soci::session& sql = lease.get();

        // 1. 查询所有已过期的 UPLOADING 临时上传会话
        soci::rowset<std::string> rows = (sql.prepare <<
            "SELECT upload_id FROM upload_sessions WHERE state = 'UPLOADING' AND expires_at < NOW()");

        for (const auto& idBinary : rows) {
            std::string idHex = bytes_to_hex(idBinary);
            std::string partPath = storageRoot_ + "/uploads/" + idHex + ".part";

            // 2. 物理擦除临时分片
            struct stat st;
            if (::stat(partPath.c_str(), &st) == 0) {
                if (::unlink(partPath.c_str()) != 0 && errno != ENOENT) {
                    std::cerr << "[Cleaner] Warning: Failed to unlink expired file: "
                              << partPath << ", error: " << ::strerror(errno) << "\n";
                }
            }

            // 3. 将会话状态在事务中更新为已失效 EXPIRED
            try {
                soci::transaction tr(sql);
                sql << "UPDATE upload_sessions SET state = 'EXPIRED' WHERE upload_id = :id",
                       soci::use(idBinary);
                tr.commit();
                successCount++;
            } catch (const std::exception& e) {
                std::cerr << "[Cleaner] Error: Failed to update database state: " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Cleaner] Critical Error: Database query failed: " << e.what() << "\n";
    }

    return successCount;
}
```

---

## 磁盘孤儿文件清理

文件在上传完毕时，采取的是**先持久化物理文件（TUS 合并和 link 固化），后写入 MySQL 关系数据**的“安全第一”写入顺序。
这保证了数据库只要有记录就一定有文件，但却留下了一个崩溃缝隙：如果物理文件刚硬链接成功，而服务器突然断电导致后续的 MySQL 事务没来得及提交，就会在磁盘上留下一个没有任何数据库记录与之关联的**孤儿物理文件**。

[ObjectOrphanReclaimer](file:///home/wxm/FileLink/src/cleaner/ObjectOrphanReclaimer.cpp#L84) 会遍历对象子目录，通过数据库比对把这些多余的残留文件安全扫除：

```cpp
std::vector<std::string> ObjectOrphanReclaimer::find_orphaned_object_paths() {
    db::SociSessionLease lease(pool_);
    db::ObjectDao objects(lease.get());
    std::vector<std::string> orphaned_paths;
    const std::string objects_root = storage_root_ + "/objects";

    // 1. 深度遍历 objects/ 两级哈希文件夹
    for (const std::string& first_level : list_directory(objects_root)) {
        if (first_level.size() != 2 || !is_lower_hex(first_level)) continue;
        const std::string first_path = objects_root + "/" + first_level;

        for (const std::string& second_level : list_directory(first_path)) {
            if (second_level.size() != 2 || !is_lower_hex(second_level)) continue;
            const std::string second_path = first_path + "/" + second_level;

            for (const std::string& hash_hex : list_directory(second_path)) {
                // 校验哈希文件名的格式完整性
                if (hash_hex.size() != 64 || !is_lower_hex(hash_hex)
                    || hash_hex.substr(0, 2) != first_level
                    || hash_hex.substr(2, 2) != second_level) {
                    continue;
                }

                const std::string object_path = second_path + "/" + hash_hex;
                if (!is_regular_file(object_path)) continue;

                db::Object object;
                // 2. 双重确认：如果在 MySQL objects 中没有该哈希的记录，说明该物理文件是事务崩溃留下的孤儿文件
                if (!objects.find(decode_hash(hash_hex), object)) {
                    orphaned_paths.push_back(object_path);
                }
            }
        }
    }
    return orphaned_paths;
}
```
在删除模式下，获取到孤儿路径列表后，系统会在执行 `::unlink` 的瞬间**再次向数据库进行一次 `find` 双重确认**，确保该物理文件在扫描期间没有被其他刚刚成功提交的合法事务所使用，完全杜绝了并发误删的危险。
