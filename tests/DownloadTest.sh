#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
mysql_password="${FILELINK_TEST_MYSQL_PASSWORD:?集成测试需要设置 FILELINK_TEST_MYSQL_PASSWORD}"
mysql_port="${FILELINK_TEST_MYSQL_PORT:-3306}"
port=$((20000 + $$ % 20000))
storage_dir="/tmp/filelink_download_test_$$"
log_file="/tmp/filelink_download_health_$$.log"

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

test_data="Hello FileLink Download! $$"
# 1. 上传文件获取 hash (采用 TUS 协议)
resp_headers=$(mktemp)
"$curl" --noproxy "*" -i -X POST -H "Upload-Length: ${#test_data}" -H "Upload-Metadata: filename ZG93bmxvYWQudHh0" --silent --show-error "http://127.0.0.1:${port}/uploads" > "$resp_headers"

location=$(grep -i "Location:" "$resp_headers" | awk '{print $2}' | tr -d '\r\n')
rm -f "$resp_headers"

if [[ -z "$location" ]]; then
    echo "Failed to get Location header for TUS session creation" >&2
    exit 1
fi

# PATCH 上传分片数据
patch_resp="$("$curl" --noproxy "*" -i -X PATCH -H "Content-Type: application/offset+octet-stream" -H "Upload-Offset: 0" -d "$test_data" --silent --show-error "$location")"

# 轮询获取会话状态，提取最终哈希值
completed=false
hash=""

for _ in {1..50}; do
    get_resp="$("$curl" --noproxy "*" --silent --show-error "$location")"
    if [[ "$get_resp" == *"\"state\":\"COMPLETED\""* ]]; then
        completed=true
        hash=$(echo "$get_resp" | grep -o '"content_hash":"[^"]*' | cut -d'"' -f4 || true)
        break
    fi
    sleep 0.05
done

if [[ "$completed" != "true" || -z "$hash" ]]; then
    echo "Upload session failed to complete or hash is empty" >&2
    exit 1
fi

echo "Uploaded with hash: $hash"

# 2. 通过 hash 下载文件
downloaded_data="$("$curl" --noproxy "*" --silent --show-error "http://127.0.0.1:${port}/objects/$hash")"

if [[ "$downloaded_data" != "$test_data" ]]; then
    echo "Download mismatch!" >&2
    echo "Expected: $test_data" >&2
    echo "Actual: $downloaded_data" >&2
    exit 1
fi

# 3. 验证无效 hash 返回 404/400
bad_resp="$("$curl" --noproxy "*" --silent --show-error --write-out "%{http_code}" -o /dev/null "http://127.0.0.1:${port}/objects/badhash")"
if [[ "$bad_resp" != "400" && "$bad_resp" != "404" ]]; then
    echo "Expected 400 or 404 for bad hash, got: $bad_resp" >&2
    exit 1
fi

echo "Download test passed!"
