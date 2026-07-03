#!/usr/bin/env bash

set -euo pipefail

server="$1"
curl="$2"
port=$((20000 + $$ % 20000))
log_file="/tmp/filelink_health_$$.log"

"$server" --address 127.0.0.1 --port "$port" --io-threads 1 >"$log_file" 2>&1 &
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
