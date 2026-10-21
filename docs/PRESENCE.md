# 当前架构说明（已替换旧 Presence/ChatPush RPC）

## ZK

仅保留：

```text
/NovaChat-service/UserServiceRpc/{ip:port}
```

用于 Gateway 发现 **UserService**，调用 `GetUserProfile`（昵称/头像/状态）。

## Redis

仅 **Pub/Sub**：跨 Gateway 在线推送私聊/群聊。

## MySQL `message`

消息先落库；离线 `status=0`；上线拉取后客户端 ACK → `status=1`。
