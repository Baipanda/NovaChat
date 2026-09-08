# Gateway 侧 RPC 调用门面（设计稿）
#
# 目标：ChatService 只依赖本门面，不直接 new MprpcChannel 散落各处。
# 等 protoc 生成 novachat::*_Stub 后，把下面注释实现补齐即可。

#pragma once

#include <string>
#include <vector>

namespace novachat {
namespace gateway {

struct LoginResult {
    int errcode = 0;
    std::string errmsg;
    int id = 0;
    std::string name;
    std::vector<std::string> offline_msgs;
};

// 薄封装：对业务屏蔽「短连接 + stub.Login」细节
class RpcClients {
public:
    // conf 里配置 user/friend/message 的 ip:port（后续可换 ZK 发现）
    explicit RpcClients(const std::string& /*rpc_conf_path*/) {}

    // 目标实现：
    //   novachat::UserServiceRpc_Stub stub(new MprpcChannel);
    //   LoginRequest req; ...
    //   stub.Login(nullptr, &req, &resp, nullptr);
    bool Login(int id, const std::string& password, LoginResult* out) {
        (void)id;
        (void)password;
        if (out) {
            out->errcode = -1;
            out->errmsg = "RpcClients::Login not wired yet — use local UserModel path";
        }
        return false;
    }
};

}  // namespace gateway
}  // namespace novachat
