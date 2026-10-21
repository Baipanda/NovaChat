#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${NOVA_DEPS_PREFIX:-$HOME/deps}"
export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export PATH="$PREFIX/bin:${PATH:-}"

docker start novachat-mysql novachat-redis6379 novachat-zk >/dev/null 2>&1 || true
pkill -x PresenceService 2>/dev/null || true
pkill -x UserService 2>/dev/null || true
pkill -x NovaGateway 2>/dev/null || true
docker rm -f novachat-nginx >/dev/null 2>&1 || true
sleep 0.3

cd "$ROOT"
./bin/UserService -i conf/user.conf >>/tmp/nova_user.log 2>&1 &
echo $! >/tmp/nova_user.pid
sleep 0.8
./bin/NovaGateway "$ROOT/web" "$ROOT/conf/gateway.conf" >>/tmp/nova_gw_run.log 2>&1 &
echo $! >/tmp/nova_gw.pid
sleep 0.8

docker run -d --name novachat-nginx --network host \
  -v "$ROOT/deploy/nginx/nginx.conf:/etc/nginx/nginx.conf:ro" \
  nginx:alpine >/dev/null 2>&1 || true

echo "UserService PID=$(cat /tmp/nova_user.pid)  Gateway PID=$(cat /tmp/nova_gw.pid)"
echo "UI: http://127.0.0.1:8088/  (或 Nginx http://127.0.0.1:8090/)"
echo "双机模拟: bash scripts/run_dual.sh"
echo "  gw-1 http://127.0.0.1:8088/   gw-2 http://127.0.0.1:8188/"
