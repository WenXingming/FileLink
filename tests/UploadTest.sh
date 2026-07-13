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

# 1. 注册并取得认证 Cookie
auth_headers=$(mktemp)
"$curl" --noproxy "*" -i -X POST \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"upload_test_${port}\",\"password\":\"correct-password\"}" \
    --silent --show-error "http://127.0.0.1:${port}/auth/register" > "$auth_headers"

session_cookie=$(awk -F': ' 'tolower($1) == "set-cookie" { sub(/\r$/, "", $2); split($2, parts, ";"); print parts[1]; exit }' "$auth_headers")
rm -f "$auth_headers"

if [[ -z "$session_cookie" ]]; then
    echo "Failed to get authentication cookie" >&2
    exit 1
fi

# 2. 初始化 TUS 会话
resp_headers=$(mktemp)
"$curl" --noproxy "*" -i -X POST -H "Cookie: $session_cookie" \
    -H "Upload-Length: 11" -H "Upload-Metadata: filename aGVsbG8udHh0" \
    --silent --show-error "http://127.0.0.1:${port}/uploads" > "$resp_headers"

location=$(grep -i "Location:" "$resp_headers" | awk '{print $2}' | tr -d '\r\n')
rm -f "$resp_headers"

if [[ -z "$location" ]]; then
    echo "Failed to get Location header for TUS session creation" >&2
    exit 1
fi

# 3. PATCH 上传分片数据
patch_resp="$("$curl" --noproxy "*" -i -X PATCH -H "Cookie: $session_cookie" \
    -H "Content-Type: application/offset+octet-stream" -H "Upload-Offset: 0" \
    -d "hello world" --silent --show-error "$location")"

# 4. 轮询获取会话状态，提取最终哈希值
expected_hash="d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24"
completed=false
hash=""

for _ in {1..50}; do
    get_resp="$("$curl" --noproxy "*" -H "Cookie: $session_cookie" --silent --show-error "$location")"
    if [[ "$get_resp" == *"\"state\":\"COMPLETED\""* ]]; then
        completed=true
        hash=$(echo "$get_resp" | grep -o '"content_hash":"[^"]*' | cut -d'"' -f4 || true)
        break
    fi
    sleep 0.05
done

if [[ "$completed" != "true" ]]; then
    echo "Upload session failed to complete in time" >&2
    exit 1
fi

if [[ "$hash" != "$expected_hash" ]]; then
    echo "Hash mismatch! Expected: $expected_hash, Got: $hash" >&2
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
