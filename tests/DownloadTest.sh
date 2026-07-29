#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
docker="$3"
mysql_password="${FILELINK_TEST_MYSQL_PASSWORD:?集成测试需要设置 FILELINK_TEST_MYSQL_PASSWORD}"
mysql_port="${FILELINK_TEST_MYSQL_PORT:-3306}"
server_port=$((20000 + $$ % 10000))
proxy_port=$((30000 + $$ % 10000))
storage_dir="/tmp/filelink_download_test_$$"
server_log="/tmp/filelink_download_server_$$.log"
nginx_log="/tmp/filelink_download_nginx_$$.log"
nginx_config="/tmp/filelink_download_nginx_$$.conf"
range_headers="/tmp/filelink_download_range_$$.headers"
range_body="/tmp/filelink_download_range_$$.body"
nginx_container="filelink-download-nginx-$$"

mkdir -p "$storage_dir"

FILELINK_MYSQL_PASSWORD="$mysql_password" \
    "$server" --address 127.0.0.1 --port "$server_port" \
    --storage-root "$storage_dir" --io-threads 1 \
    --mysql-port "$mysql_port" >"$server_log" 2>&1 &
server_pid=$!

cleanup() {
    "$docker" stop --time 1 "$nginx_container" >/dev/null 2>&1 || true
    if [[ -n "${nginx_pid:-}" ]]; then
        wait "$nginx_pid" 2>/dev/null || true
    fi
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    rm -f "$server_log" "$nginx_log" "$nginx_config" "$range_headers" "$range_body"
    rm -rf "$storage_dir"
}
trap cleanup EXIT

# 等待服务器启动
for _ in {1..50}; do
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$server_log"
        exit 1
    fi

    if "$curl" --noproxy "*" --silent --show-error --max-time 1 "http://127.0.0.1:${server_port}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.05
done

{
    printf 'server {\n'
    printf '    listen %s;\n' "$proxy_port"
    printf '    server_name _;\n'
    printf '    location /_filelink_objects/ {\n'
    printf '        internal;\n'
    printf '        alias /srv/filelink/storage/objects/;\n'
    printf '    }\n'
    printf '    location / {\n'
    printf '        proxy_pass http://127.0.0.1:%s;\n' "$server_port"
    printf '        proxy_set_header Host $http_host;\n'
    printf '    }\n'
    printf '}\n'
} >"$nginx_config"

"$docker" run --rm --pull never --name "$nginx_container" --network host \
    -v "$nginx_config:/etc/nginx/conf.d/default.conf:ro" \
    -v "$storage_dir:/srv/filelink/storage:ro" \
    nginx:trixie-perl >"$nginx_log" 2>&1 &
nginx_pid=$!

for _ in {1..50}; do
    if ! kill -0 "$nginx_pid" 2>/dev/null; then
        cat "$nginx_log"
        exit 1
    fi

    if "$curl" --noproxy "*" --silent --show-error --max-time 1 "http://127.0.0.1:${proxy_port}/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.05
done

base_url="http://127.0.0.1:${proxy_port}"
test_data="Hello FileLink Download! $$"
# 1. 注册并取得认证 Cookie
auth_headers=$(mktemp)
"$curl" --noproxy "*" -i -X POST \
    -H "Content-Type: application/json" \
    -d "{\"username\":\"download_test_${server_port}\",\"password\":\"correct-password\"}" \
    --silent --show-error "$base_url/auth/register" > "$auth_headers"

session_cookie=$(awk -F': ' 'tolower($1) == "set-cookie" { sub(/\r$/, "", $2); split($2, parts, ";"); print parts[1]; exit }' "$auth_headers")
rm -f "$auth_headers"

if [[ -z "$session_cookie" ]]; then
    echo "Failed to get authentication cookie" >&2
    exit 1
fi

# 2. 上传文件并取得逻辑文件 ID（采用 TUS 协议）
resp_headers=$(mktemp)
"$curl" --noproxy "*" -i -X POST -H "Cookie: $session_cookie" \
    -H "Upload-Length: ${#test_data}" -H "Upload-Metadata: filename ZG93bmxvYWQudHh0" \
    --silent --show-error "$base_url/uploads" > "$resp_headers"

location=$(grep -i "Location:" "$resp_headers" | awk '{print $2}' | tr -d '\r\n')
rm -f "$resp_headers"

if [[ -z "$location" ]]; then
    echo "Failed to get Location header for TUS session creation" >&2
    exit 1
fi

# PATCH 上传分片数据
patch_resp="$("$curl" --noproxy "*" -i -X PATCH -H "Cookie: $session_cookie" \
    -H "Content-Type: application/offset+octet-stream" -H "Upload-Offset: 0" \
    -d "$test_data" --silent --show-error "$location")"

# 轮询获取会话状态，提取最终逻辑文件 ID
completed=false
file_id=""

for _ in {1..50}; do
    get_resp="$("$curl" --noproxy "*" -H "Cookie: $session_cookie" --silent --show-error "$location")"
    if [[ "$get_resp" == *"\"state\":\"COMPLETED\""* ]]; then
        completed=true
        file_id=$(echo "$get_resp" | grep -o '"file_id":"[^"]*' | cut -d'"' -f4 || true)
        break
    fi
    sleep 0.05
done

if [[ "$completed" != "true" || -z "$file_id" ]]; then
    echo "Upload session failed to complete or file ID is empty" >&2
    exit 1
fi

echo "Uploaded file: $file_id"

# 3. 通过私有文件地址下载文件
downloaded_data="$("$curl" --noproxy "*" --silent --show-error \
    -H "Cookie: $session_cookie" \
    "$base_url/files/$file_id/download")"

if [[ "$downloaded_data" != "$test_data" ]]; then
    echo "Download mismatch!" >&2
    echo "Expected: $test_data" >&2
    echo "Actual: $downloaded_data" >&2
    exit 1
fi

# 4. 验证 Nginx 提供字节范围下载
range_status="$("$curl" --noproxy "*" --silent --show-error \
    --range 0-4 --dump-header "$range_headers" --output "$range_body" \
    --write-out "%{http_code}" -H "Cookie: $session_cookie" \
    "$base_url/files/$file_id/download")"
range_data="$(<"$range_body")"
content_range="$(awk -F': ' 'tolower($1) == "content-range" { sub(/\r$/, "", $2); print $2; exit }' "$range_headers")"

if [[ "$range_status" != "206" || "$range_data" != "Hello" \
    || "$content_range" != "bytes 0-4/${#test_data}" ]]; then
    echo "Range download mismatch!" >&2
    echo "Status: $range_status" >&2
    echo "Content-Range: $content_range" >&2
    echo "Body: $range_data" >&2
    exit 1
fi

# 5. 验证客户端不能绕过授权直接访问内部对象地址
object_path="$(find "$storage_dir/objects" -type f -print -quit)"
if [[ -z "$object_path" ]]; then
    echo "Uploaded object was not found in storage" >&2
    exit 1
fi
object_key="${object_path#"$storage_dir/objects/"}"
direct_object_status="$("$curl" --noproxy "*" --silent --show-error \
    --write-out "%{http_code}" -o /dev/null \
    "$base_url/_filelink_objects/$object_key")"
if [[ "$direct_object_status" != "404" ]]; then
    echo "Expected 404 for direct internal object access, got: $direct_object_status" >&2
    exit 1
fi

# 6. 验证私有下载拒绝未认证请求
unauthenticated_status="$("$curl" --noproxy "*" --silent --show-error --write-out "%{http_code}" -o /dev/null \
    "$base_url/files/$file_id/download")"
if [[ "$unauthenticated_status" != "401" ]]; then
    echo "Expected 401 for unauthenticated download, got: $unauthenticated_status" >&2
    exit 1
fi

# 7. 创建公开分享并在无认证状态下通过 Nginx 下载
share_response="$("$curl" --noproxy "*" --silent --show-error -X POST \
    -H "Cookie: $session_cookie" -H "Content-Type: application/json" \
    -d '{"expires_in_seconds":3600}' \
    "$base_url/files/$file_id/shares")"
share_token="$(echo "$share_response" | grep -o '"token":"[^"]*' | cut -d'"' -f4 || true)"
if [[ -z "$share_token" ]]; then
    echo "Failed to create public share: $share_response" >&2
    exit 1
fi

shared_data="$("$curl" --noproxy "*" --silent --show-error \
    "$base_url/shares/$share_token/download")"
if [[ "$shared_data" != "$test_data" ]]; then
    echo "Public share download mismatch!" >&2
    echo "Expected: $test_data" >&2
    echo "Actual: $shared_data" >&2
    exit 1
fi

echo "Download test passed!"
