#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
mysql_password="${FILELINK_TEST_MYSQL_PASSWORD:?集成测试需要设置 FILELINK_TEST_MYSQL_PASSWORD}"
mysql_port="${FILELINK_TEST_MYSQL_PORT:-3306}"
port=$((20000 + $$ % 20000))
log_file="/tmp/filelink_health_$$.log"

FILELINK_MYSQL_PASSWORD="$mysql_password" \
    "$server" --address 127.0.0.1 --port "$port" --io-threads 1 \
    --mysql-port "$mysql_port" >"$log_file" 2>&1 &
server_pid=$!

cleanup() {
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    rm -f "$log_file"
}
trap cleanup EXIT

response=""
for _ in {1..50}; do
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$log_file"
        exit 1
    fi

    if response="$("$curl" --noproxy "*" --silent --show-error --max-time 1 \
        --write-out $'\n%{http_code}' "http://127.0.0.1:${port}/health" 2>/dev/null)"; then
        break
    fi
    sleep 0.05
done

expected=$'{"status":"ok"}\n200'
if [[ "$response" != "$expected" ]]; then
    cat "$log_file"
    echo "unexpected response: $response" >&2
    exit 1
fi
