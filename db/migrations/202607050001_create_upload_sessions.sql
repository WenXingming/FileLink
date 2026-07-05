-- migrate:up transaction:false
CREATE TABLE upload_sessions (
    upload_id BINARY(16) NOT NULL,
    state VARCHAR(16) NOT NULL,
    file_name VARCHAR(255) NOT NULL,
    total_size BIGINT UNSIGNED NOT NULL,
    committed_offset BIGINT UNSIGNED NOT NULL DEFAULT 0,
    expected_hash BINARY(32) NULL,
    content_hash BINARY(32) NULL,
    failure_reason VARCHAR(255) NULL,
    created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
    updated_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6)
        ON UPDATE CURRENT_TIMESTAMP(6),
    expires_at DATETIME(6) NOT NULL,
    PRIMARY KEY (upload_id),
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
    INDEX idx_upload_expiry (state, expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- migrate:down transaction:false
DROP TABLE upload_sessions;
