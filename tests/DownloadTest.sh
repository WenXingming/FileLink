#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
port=$((20000 + $$ % 20000))
storage_dir="/tmp/filelink_download_test_$$"
log_file="/tmp/filelink_download_health_$$.log"

mkdir -p "$storage_dir"

"$server" --address 127.0.0.1 --port "$port" --storage-root "$storage_dir" --io-threads 1 >"$log_file" 2>&1 &
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
# 1. 上传文件获取 hash
upload_resp="$("$curl" --noproxy "*" -X POST -d "$test_data" --silent --show-error "http://127.0.0.1:${port}/upload")"
hash=$(echo "$upload_resp" | grep -o '"hash":"[^"]*' | cut -d'"' -f4 || true)

if [[ -z "$hash" ]]; then
    echo "Upload failed, could not parse hash from: $upload_resp" >&2
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
