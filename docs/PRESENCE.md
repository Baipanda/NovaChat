# Presence RPC 骨架说明

## 角色

```text
Chat Node                          PresenceService
  login ──Register(uid,gw)──►        map[uid]= {online, gateway_id}
  logout ─Unregister────────►        erase if same gw
  ?路由 ──Query(uid)────────►        返回 online + gateway_id
  发消息 ─ Redis PUBLISH ──► 其他 Chat（不经 Presence 传正文）
```

## 文件

| 路径 | 作用 |
|------|------|
| `proto/presence.proto` | IDL：Register / Unregister / Query / Kick |
| `services/presence/presence_store.*` | 内存路由表 |
| `services/presence/presence_service.hpp` | 业务方法 + 如何挂 mprpc 的注释 |
| `services/presence/main.cpp` | 进程入口；当前本地自测，Run() 待接线 |
| `gateway/include/presence_client.hpp` | Chat 侧调用门面与挂载点注释 |

## 与原版关系

- **保留**：`_userConnMap`、MySQL `state`、Redis 跨机推送  
- **新增**：可查询的 `uid → gateway_id`（原版没有）  
- **不替代**：用 Presence 传聊天消息

## 生成桩后接线

```bash
protoc -I proto --cpp_out=proto proto/common.proto proto/presence.proto
```

然后让 `PresenceService` 继承 `novachat::PresenceServiceRpc`，`main` 里 `NotifyService` + `Run()`。
