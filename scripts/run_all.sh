#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${NOVA_DEPS_PREFIX:-$HOME/deps}"
export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export PATH="$PREFIX/bin:${PATH:-}"

# 确保依赖容器在跑（幂等）
docker start novachat-mysql novachat-redis6379 novachat-zk >/dev/null 2>&1 || true

pkill -x PresenceService 2>/dev/null || true
pkill -x NovaGateway 2>/dev/null || true
docker rm -f novachat-nginx >/dev/null 2>&1 || true
sleep 0.3

cd "$ROOT"
./bin/PresenceService -i conf/presence.conf >>/tmp/nova_presence.log 2>&1 &
echo $! >/tmp/nova_presence.pid
sleep 1.0
./bin/NovaGateway "$ROOT/web" "$ROOT/conf/gateway.conf" >>/tmp/nova_gw_run.log 2>&1 &
echo $! >/tmp/nova_gw.pid
sleep 0.5

# Nginx 反代：对外统一入口 :8090（HTTP + /ws）
docker run -d --name novachat-nginx --network host \
  -v "$ROOT/deploy/nginx/nginx.conf:/etc/nginx/nginx.conf:ro" \
  nginx:alpine >/dev/null

echo "Presence PID=$(cat /tmp/nova_presence.pid)  Gateway PID=$(cat /tmp/nova_gw.pid)"
echo "入口(Nginx): http://127.0.0.1:8090/   WS: ws://127.0.0.1:8090/ws"
echo "直连 Gateway: http://127.0.0.1:8088/  WS: ws://127.0.0.1:8089/"
echo "PresenceRPC: 127.0.0.1:8010   ZooKeeper: 127.0.0.1:2181"
