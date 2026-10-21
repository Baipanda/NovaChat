#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${NOVA_DEPS_PREFIX:-$HOME/deps}"
export PATH="$PREFIX/bin:${PATH:-}"
export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

mkdir -p "$ROOT/build"
cd "$ROOT/build"
cmake .. -DNOVA_DEPS_PREFIX="$PREFIX" -DNOVA_BUILD_RPC=ON
make -j"$(nproc)"
echo "OK -> bin/NovaGateway bin/NovaClient bin/UserService"
