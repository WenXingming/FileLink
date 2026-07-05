#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="${project_root}/.env"
server="${project_root}/build/src/filelink-server"

if [[ ! -r "${env_file}" ]]; then
    echo "filelink: 未找到 .env，请先执行 cp .env.example .env 并填写配置" >&2
    exit 1
fi

if [[ ! -x "${server}" ]]; then
    echo "filelink: 未找到可执行文件，请先执行 cmake --build build" >&2
    exit 1
fi

cd "${project_root}"
set -a
# shellcheck disable=SC1090
source "${env_file}"
set +a

exec "${server}" --config "${project_root}/config/server.toml" "$@"
