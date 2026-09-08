#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${NOVA_DEPS_PREFIX:-$HOME/deps}"
export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
cd "$ROOT"
# 若已有旧进程占用端口，先提示
exec ./bin/NovaGateway "$ROOT/web"
