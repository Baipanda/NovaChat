# NovaChat 简历（单项目 · 经典体例）

**时间：** 2025.12 - 2026.02（可按实际改）　**角色：** 独立开发  
**开发环境：** CentOS / Ubuntu、VS Code 远程开发、g++、gdb、CMake、Git  
**项目介绍：** 基于 muduo 的分布式集群聊天系统 NovaChat：Gateway 提供长连接与聊天业务，自研 RPC + ZooKeeper 承载 Presence 在线路由，并结合 MySQL / Redis / Nginx，支持用户注册登录、好友与群组、实时与离线消息，以及多端接入与水平扩展。  
**技术栈：** muduo、Protobuf、ZooKeeper、Json、MySQL、Redis、Nginx、CMake、Socket / RPC

**主要工作：**

● 基于 muduo 构建 Gateway 网络通信，利用 Reactor（epoll + 线程池、one loop per thread）支撑高并发长连接；业务消息用 Json 序列化传输，提供 TCP 终端客户端，并自研 WebSocket 接入浏览器，复用同一套 ChatService。

● 自主实现 RPC 框架：基于 Protobuf 做序列化，服务端继承生成 Service 发布业务，客户端 Stub 屏蔽网络细节；在 NovaChat 中用于 Presence（登录注册在线、好友在线查询等），与聊天主路径解耦。

● 使用 ZooKeeper 完成 RPC 服务注册与发现：Presence 以临时节点登记 `ip:port`，Gateway 拉取子节点寻址，节点宕机后临时节点自动消失；发现失败回退配置，保证可联通。

● 使用 MySQL 存储用户、好友、群组与离线消息；基于 Redis 发布-订阅解决跨 Gateway 消息转发；Nginx 反向代理 HTTP/WebSocket 统一入口，多节点场景下结合 Redis 转发支撑水平扩展。

---

## 可直接粘贴版

开发环境：CentOS、VS Code远程开发、g++、gdb、CMake、Git  
项目介绍：基于muduo的分布式集群聊天系统NovaChat，Gateway提供长连接与聊天业务，自研RPC+ZooKeeper承载Presence在线路由，并结合MySQL/Redis/Nginx，支持用户注册登录、好友/群组管理、实时与离线消息、多端接入与水平扩展。  
主要工作：  
● 基于muduo构建Gateway网络通信，利用Reactor模型（epoll+线程池、one loop per thread）实现高并发长连接；业务消息基于Json序列化传输，支持TCP终端与自研WebSocket浏览器双端，复用同一套ChatService  
● 自主实现RPC框架：基于Protobuf完成序列化/反序列化，服务端继承Protobuf生成的Service类发布业务，客户端通过Stub发起调用并屏蔽底层网络细节；在本项目中用于Presence在线路由（登录注册、好友在线查询等），与聊天主路径解耦  
● 使用ZooKeeper完成RPC服务节点的注册与发现：服务以临时节点保存ip:port，调用方拉取子节点寻址；会话断开后节点自动删除，发现失败则回退本地配置  
● 使用MySQL存储用户信息、好友关系、群组数据与离线消息；基于Redis发布-订阅解决用户连接不同Gateway时的跨机消息转发；配置Nginx反向代理HTTP/WebSocket统一入口，多Gateway场景下结合Redis转发支持水平扩展  
技术栈：muduo网络库、Protobuf、ZooKeeper、Json、MySQL、Redis、Nginx、CMake、RPC框架
