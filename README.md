# NovaChat

**集群 IM（C++）+ 自研 RPC（目录 `rpc/`，原 mprpc）+ Presence 在线路由**

```text
ChatClient / Web
     │ TCP:6666 或 HTTP:8088 + WS:8089
     ▼
NovaGateway ──本地──► MySQL（账号/好友/群/离线）
     │                Redis（跨机推消息）
     │
     └── 自研 RPC ──► PresenceService(:8010)
                      Register / Unregister / Query / List
```

## 启动

```bash
bash scripts/build.sh
bash scripts/run_all.sh   # PresenceService + NovaGateway
```

| 入口 | 地址 |
|------|------|
| 浏览器 | http://127.0.0.1:8088/（需转发 8088+8089） |
| 终端客户端 | `./bin/NovaClient 127.0.0.1 6666` |
| Presence RPC | `127.0.0.1:8010` |

种子账号：`1/2/3` = alice/bob/carol，密码 `123456`。

## 在线状态怎么来的

- 登录/下线：Gateway 调 **Presence.Register / Unregister**
- 好友列表里的 online/offline：优先 **Presence.Query**（失败才回退 MySQL）
- 有人上下线：Gateway **List + 推送** `type=presence` 给网页刷新绿点

聊天正文仍走长连接 / Redis，**不用 RPC 传消息**。

## 目录

```text
rpc/                 # 自研 RPC 框架（原 mprpc）
services/presence/   # PresenceService 进程
gateway/             # NovaGateway（Chat + Web + RPC 客户端）
client/              # NovaClient
proto/presence.proto
conf/presence.conf   # Presence 监听
conf/gateway.conf    # Gateway 作为 RPC 调用方指向 Presence
```
