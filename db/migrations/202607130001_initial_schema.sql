-- migrate:up transaction:false

-- 用户是文件归属与授权的主体；password_hash 仅保存不可逆密码哈希。
CREATE TABLE users (
    user_id BINARY(16) NOT NULL,
    username VARCHAR(64) NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
        ON UPDATE CURRENT_TIMESTAMP(6),
    disabled_at DATETIME(6) NULL,
    PRIMARY KEY (user_id),
    UNIQUE KEY uq_users_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- Object 是内容寻址存储中的唯一、不可变字节序列；物理路径由 content_hash 推导。
CREATE TABLE objects (
    content_hash BINARY(32) NOT NULL,
    byte_size BIGINT UNSIGNED NOT NULL,
    ref_count BIGINT UNSIGNED NOT NULL DEFAULT 0,
    state VARCHAR(16) NOT NULL DEFAULT 'READY',
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    PRIMARY KEY (content_hash),
    CONSTRAINT chk_object_state CHECK (state IN ('READY', 'PENDING_DELETE'))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- File 是用户看到的逻辑文件，多个 File 可引用同一去重 Object。
CREATE TABLE files (
    file_id BINARY(16) NOT NULL,
    owner_user_id BINARY(16) NOT NULL,
    content_hash BINARY(32) NOT NULL,
    display_name VARCHAR(255) NOT NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    PRIMARY KEY (file_id),
    CONSTRAINT fk_files_owner
        FOREIGN KEY (owner_user_id) REFERENCES users (user_id),
    CONSTRAINT fk_files_object
        FOREIGN KEY (content_hash) REFERENCES objects (content_hash),
    INDEX idx_files_owner_created (owner_user_id, created_at),
    INDEX idx_files_object (content_hash)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- UploadSession 只描述临时传输过程；成功后通过 completed_file_id 指向最终逻辑文件。
CREATE TABLE upload_sessions (
    upload_id BINARY(16) NOT NULL,
    owner_user_id BINARY(16) NULL,
    state VARCHAR(16) NOT NULL,
    file_name VARCHAR(255) NOT NULL,
    total_size BIGINT UNSIGNED NOT NULL,
    committed_offset BIGINT UNSIGNED NOT NULL DEFAULT 0,
    expected_hash BINARY(32) NULL,
    content_hash BINARY(32) NULL,
    completed_file_id BINARY(16) NULL,
    failure_reason VARCHAR(255) NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
        ON UPDATE CURRENT_TIMESTAMP(6),
    expires_at DATETIME(6) NOT NULL,
    PRIMARY KEY (upload_id),
    CONSTRAINT fk_upload_sessions_owner
        FOREIGN KEY (owner_user_id) REFERENCES users (user_id),
    CONSTRAINT fk_upload_sessions_completed_file
        FOREIGN KEY (completed_file_id) REFERENCES files (file_id)
        ON DELETE SET NULL,
    CONSTRAINT chk_upload_offset
        CHECK (committed_offset <= total_size),
    CONSTRAINT chk_upload_state
        CHECK (state IN (
            'UPLOADING',
            'FINALIZING',
            'COMPLETED',
            'FAILED',
            'ABORTED',
            'EXPIRED'
        )),
    CONSTRAINT chk_completed_hash
        CHECK (state <> 'COMPLETED' OR content_hash IS NOT NULL),
    INDEX idx_upload_expiry (state, expires_at),
    INDEX idx_upload_sessions_owner_created (owner_user_id, created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- migrate:down transaction:false
DROP TABLE upload_sessions;
DROP TABLE files;
DROP TABLE objects;
DROP TABLE users;
