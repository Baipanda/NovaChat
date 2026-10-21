# NovaChat 架构（面试口述版）

## 分工

| 组件 | 职责 |
|------|------|
| **ZooKeeper** | 只登记 `UserServiceRpc` 实例临时节点 |
| **UserService** | MySQL 查用户资料；供 Gateway **RPC 调用** `GetUserProfile`（昵称、头像、状态） |
| **Gateway** | 长连接、登录、消息；**消息不走 RPC** |
| **Redis Pub/Sub** | 接收方在线时跨机实时推送 |
| **MySQL `message`** | 消息先落库；离线保留；上线拉取后 ACK 更新状态 |

## 消息路径（图 2）

1. Client A → Gateway：收消息  
2. **先写入 MySQL**（from/to/content/status）  
3. 在线：本机直推或 **Redis PUBLISH**  
4. 离线：status=pending，等上线  
5. Client B 上线：拉未读 → 推送 → 客户端 **OFFLINE_MSG_ACK** → status=delivered  

## RPC 路径（图 1）

登录时 Gateway 经 ZK 发现 UserService，调用 `GetUserProfile`（request_id 匹配响应）；好友关系本地查库，再对每个好友 RPC 拉资料。

## 前端

基于 [Aura-Chat](https://github.com/arikmukherjee/Aura-Chat)（MIT）改造：头像好友列表 + ZK 节点演示面板。
