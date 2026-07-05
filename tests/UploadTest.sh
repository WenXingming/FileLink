#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
mysql_password="${FILELINK_TEST_MYSQL_PASSWORD:?集成测试需要设置 FILELINK_TEST_MYSQL_PASSWORD}"
mysql_port="${FILELINK_TEST_MYSQL_PORT:-3306}"
port=$((20000 + $$ % 20000))
# 使用单独的 storage root 避免冲突
storage_dir="/tmp/filelink_upload_test_$$"
log_file="/tmp/filelink_upload_health_$$.log"

mkdir -p "$storage_dir"

FILELINK_MYSQL_PASSWORD="$mysql_password" \
    "$server" --address 127.0.0.1 --port "$port" \
    --storage-root "$storage_dir" --io-threads 1 \
    --mysql-port "$mysql_port" >"$log_file" 2>&1 &
server_pid=$!

cleanup() {
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    rm -f "$log_file"
    rm -rf "$storage_dir"
}
trap cleanup EXIT

# 等待服务器启动
for _ in {1..50}; do
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$log_file"
        exit 1
    fi

    if "$curl" --noproxy "*" --silent --show-error --max-time 1 "http://127.0.0.1:${port}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.05
done

# 上传测试数据 "hello world"
# "hello world" hash is "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24"
response="$("$curl" --noproxy "*" -X POST -d "hello world" --silent --show-error "http://127.0.0.1:${port}/upload")"

expected_hash="d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24"

if [[ "$response" != *"\"hash\":\"$expected_hash\""* ]]; then
    echo "Upload failed or wrong hash returned: $response" >&2
    exit 1
fi

if [[ "$response" != *"\"status\":\"success\""* ]]; then
    echo "Upload status not success: $response" >&2
    exit 1
fi

# 验证文件是否真正在存储里
obj_path="${storage_dir}/objects/d7/49/${expected_hash}"
if [[ ! -f "$obj_path" ]]; then
    echo "Object file not found at: $obj_path" >&2
    exit 1
fi

actual_content="$(cat "$obj_path")"
if [[ "$actual_content" != "hello world" ]]; then
    echo "Object content mismatch" >&2
    exit 1
fi

echo "Upload test passed!"
