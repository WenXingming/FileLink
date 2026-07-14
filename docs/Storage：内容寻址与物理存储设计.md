# Storage：内容寻址与物理存储设计

在 FileLink 系统中，[src/storage/](file:///home/wxm/FileLink/src/storage/) 模块负责底层的物理存储，是整个系统“物理去重（秒传）”与“内容寻址存储（CAS）”的基石。该模块将业务层的“逻辑文件”与底层的“物理对象”彻底解耦，使系统能够实现极高的存储空间效率与断电可靠性。

本篇文档将详细阐述 `storage` 模块的物理存储结构、原子发布机制、去重秒传流程及断电容灾设计。

---

## 模块职责与接口

物理存储模块的核心实现是 [ObjectStore](file:///home/wxm/FileLink/src/storage/ObjectStore.h#L26) 类，它不维护任何数据库状态，仅封装了针对底层 POSIX 文件系统的物理操作。其主要接口定义如下：

* **哈希寻址（[get_object_path](file:///home/wxm/FileLink/src/storage/ObjectStore.h#L32)）**：根据文件内容的 64 位小写十六进制 BLAKE3 哈希值，直接计算出该文件在服务器磁盘上的绝对物理路径。
* **原子发布（[commit](file:///home/wxm/FileLink/src/storage/ObjectStore.h#L30)）**：将临时上传文件原子化地“发布”为正式的内容寻址物理对象。发布成功后返回 [CommitResult](file:///home/wxm/FileLink/src/storage/ObjectStore.h#L18)，标明该对象是被首次创建（`Created`）还是被直接复用（`Reused`）。

---

## 物理存储目录分层

为了防范在 Linux 文件系统（如 ext4）中单一目录下文件过多导致的索引检索变慢（目录膨胀），`ObjectStore` 采用了两级子目录分流的哈希分片存储结构：

```text
storage/
└── objects/                      # 物理对象根目录
    └── ab/                       # 第一级目录：哈希前两位字符 (256 种可能)
        └── cd/                   # 第二级目录：哈希第三、四位字符 (256 种可能)
            └── abcd9f81e3...     # 正式物理文件：完整的 64 位 BLAKE3 哈希文件名
```

通过这种分布方式，系统在磁盘上最多会创建 $256 \times 256 = 65,536$ 个叶子文件夹。即便物理对象的规模达到数百万级，每个叶子文件夹下的平均文件数量也会被稀释到数十个以内，从而完美地保护了文件系统的读写性能。

---

## 原子发布与秒传流程

当分片上传完成并拼接出完整的临时文件后，系统会通过硬链接（Hard Link）实现物理对象的秒传与原子发布。整个调用流转如下图所示：

```text
 临时文件拼接完成 (tempPath)
             │
             ▼
    ObjectStore::commit
             │
      1. 执行 fdatasync(tempPath)
      确保临时文件数据与属性强制刷盘 (避免虚无指向)
             │
             ▼
      2. 执行 POSIX link(tempPath, objectPath)
      原子创建指向正式哈希路径的硬链接
             │
             ├──────────────────────────┐
             ▼ (返回 0，硬链接成功)        ▼ (返回 EEXIST，物理对象已存在)
      [ 首次创建：Created ]         [ 物理复用：Reused ]
             │                          │
      fsync(secondLevelDir)             │
      确保父目录项抗掉电持久化           │
             │                          │
             ▼                          ▼
      3. unlink(tempPath)           3. unlink(tempPath)
      擦除临时文件路径引用           擦除临时文件路径引用
             │                          │
             ▼                          ▼
      返回 Created 状态             返回 Reused 状态 (秒传成功)
```

### 硬链接秒传原理与代码实现

整个硬链接秒传与去重的逻辑，是由**物理存储层（`ObjectStore`）**与**上层业务服务（`UploadService`）**双层协作完成的：

* **物理层的硬链接冲突拦截**：在 [ObjectStore.cpp](file:///home/wxm/FileLink/src/storage/ObjectStore.cpp#L130-L141) 的 `commit` 方法中，系统利用 POSIX `::link(tempPath, objectPath)` 系统调用的原子性进行判定。如果创建硬链接失败且 `errno` 报告为 `EEXIST`（表示目标哈希物理对象在系统里已存在），系统会执行 `::unlink` 擦除刚刚拼装好的多余临时文件，并向业务层返回 `CommitStatus::Reused` 状态。

  ```cpp
  // src/storage/ObjectStore.cpp 中 commit 函数核心片段
  if (::link(tempPath.c_str(), objectPath.c_str()) == 0) {
      sync_directory(secondLevelDir);
      (void)::unlink(tempPath.c_str());
      return CommitResult{ CommitStatus::Created, objectPath };
  }

  const int linkError = errno;
  if (linkError == EEXIST) {
      (void)::unlink(tempPath.c_str()); // 物理去重：安全删除多余临时文件
      return CommitResult{ CommitStatus::Reused, objectPath }; // 告知上层可直接复用已有物理文件
  }
  ```
* **业务层的引用计数与元数据装配**：在 [UploadService.cpp](file:///home/wxm/FileLink/src/uploads/UploadService.cpp#L509-L516) 判定物理层提交成功后，会在 `complete_published_session` 中启动 MySQL 事务，调用 `db::ObjectDao::add_reference` 递增该物理对象的 `reference_count` 引用计数，并在 `files` 表中插入新的逻辑文件记录，建立“逻辑 UUID 到物理 BLAKE3 哈希”的关联绑定，最后完成会话更新并提交整个事务。

  ```cpp
  // src/uploads/UploadService.cpp 中 complete_published_session 核心片段
  db::SociSessionLease lease(pool_);
  soci::session& sql = lease.get();
  soci::transaction transaction(sql);

  // 1. 递增物理对象的引用计数 (引用计数 +1，防并发删除冲突)
  const std::string hashBytes = hex_to_bytes(realHashHex);
  if (db::ObjectDao(sql).add_reference(hashBytes, session.total_size)
      == db::ObjectReferenceResult::Reclaiming) {
      return false; // 如果该对象正处于离线回收阶段，拒绝绑定并重试
  }

  // 2. 插入逻辑文件记录，建立属于当前用户的逻辑映射
  db::File file;
  file.file_id = generate_random_uuid_binary();
  file.owner_user_id = session.owner_user_id;
  file.content_hash = hashBytes;
  file.display_name = session.file_name;
  db::FileDao(sql).create(file);

  transaction.commit(); // 提交事务，完成发布
  ```

这种分层设计既实现了无竞态条件的极速秒传，又利用数据库的引用计数有效防止了物理对象被并发删除，确保了双端一致性。

### 保证掉电一致性

* **先刷数据，后链名字**：在执行 `link` 之前，必须先对临时文件描述符调用 `fdatasync`。这保证了即使在此期间服务器突然断电，也绝对不会发生“目录里多出了哈希文件名字，但其内部数据却是一片空白”的损坏状态。
* **目录刷盘安全锁**：当成功创建硬链接后，必须对包含该文件的二级子目录执行 `fsync`，将新的目录项结构安全地固化在磁盘介质上。

---

## 逻辑与物理概念对照

在系统日常的增删改查中，“逻辑文件”与“物理对象”在底层扮演着完全不同的角色，它们的具体职责区分如下表所示：


| 维度           | 逻辑文件 (File)                            | 物理对象 (Object)                                   |
| :--------------- | :------------------------------------------- | :---------------------------------------------------- |
| **所属层级**   | 数据库逻辑层（MySQL`files` 表）            | 磁盘文件系统层（`storage/` 目录）                   |
| **主键标志**   | 随机 UUID（`file_id`）                     | 内容唯一哈希（BLAKE3）                              |
| **元数据内容** | 用户名、自定义显示文件名（`display_name`） | 文件物理字节大小（`size`）                          |
| **删除行为**   | 物理删除数据库记录                         | 引用计数归零后，由 Cleaner 离线物理回收（`unlink`） |
| **一致性控制** | 数据库本地事务                             | 读写锁与原子硬链接机制                              |

这种高度解耦的设计极大地降低了数据丢失和损坏的风险，使得上层业务开发可以非常灵活地变更逻辑文件名，而完全不用担心损坏物理磁盘的引用。
