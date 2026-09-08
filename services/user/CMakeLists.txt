# UserService：账号登录 / 注册 / 状态（目标：承接 Gateway 的 RPC 调用）
#
# 正式接入步骤：
#   1. protoc 生成 user.pb.cc/h
#   2. 将 gateway 的 Usermodel / OfflineMsgModel / FriendModel 查询迁到本进程
#   3. Gateway ChatService::login 改为 stub.Login(...)
#
# 当前为可编译的进程骨架，打印启动说明，便于先看清仓库形态。

add_executable(UserService main.cpp)
target_link_libraries(UserService mprpc muduo_net muduo_base protobuf pthread)
