-- migrate:up transaction:false

-- 认证会话和公开分享的生命周期完全由数据库时间决定，交给 MySQL 自行清理。
CREATE EVENT IF NOT EXISTS cleanup_expired_user_sessions
    ON SCHEDULE EVERY 1 DAY
    DO
        DELETE FROM user_sessions WHERE expires_at <= NOW(6);

CREATE EVENT IF NOT EXISTS cleanup_expired_shares
    ON SCHEDULE EVERY 1 DAY
    DO
        DELETE FROM shares WHERE expires_at <= NOW(6);

-- migrate:down

DROP EVENT IF EXISTS cleanup_expired_shares;
DROP EVENT IF EXISTS cleanup_expired_user_sessions;
