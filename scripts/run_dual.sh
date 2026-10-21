#!/usr/bin/env bash
# 双 Gateway 模拟：alice 连 gw-1，bob 连 gw-2，消息经 Redis Pub/Sub 跨机
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${NOVA_DEPS_PREFIX:-$HOME/deps}"
export LD_LIBRARY_PATH="$PREFIX/lib:$PREFIX/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export PATH="$PREFIX/bin:${PATH:-}"

docker start novachat-mysql novachat-redis6379 novachat-zk >/dev/null 2>&1 || true
pkill -x UserService 2>/dev/null || true
pkill -x NovaGateway 2>/dev/null || true
docker rm -f novachat-nginx >/dev/null 2>&1 || true
sleep 0.3

cd "$ROOT"
./bin/UserService -i conf/user.conf >>/tmp/nova_user.log 2>&1 &
echo $! >/tmp/nova_user.pid
sleep 0.6

./bin/NovaGateway "$ROOT/web" "$ROOT/conf/gateway.conf" >>/tmp/nova_gw1.log 2>&1 &
echo $! >/tmp/nova_gw1.pid
./bin/NovaGateway "$ROOT/web" "$ROOT/conf/gateway2.conf" >>/tmp/nova_gw2.log 2>&1 &
echo $! >/tmp/nova_gw2.pid
sleep 0.6

echo "UserService=$(cat /tmp/nova_user.pid)"
echo "Gateway1(gw-1)=$(cat /tmp/nova_gw1.pid)  http://127.0.0.1:8088/  ws://127.0.0.1:8089/"
echo "Gateway2(gw-2)=$(cat /tmp/nova_gw2.pid)  http://127.0.0.1:8188/  ws://127.0.0.1:8189/"
echo ""
echo "模拟跨机："
echo "  浏览器A打开 http://127.0.0.1:8088/  登录用户1"
echo "  浏览器B打开 http://127.0.0.1:8188/  登录用户2"
echo "  互发消息即走 Redis Pub/Sub 跨 Gateway"
