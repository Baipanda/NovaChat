#include "user_rpc_client.hpp"
#include "mprpcapplication.h"
#include "mprpcchannel.h"
#include "mprpccontroller.h"
#include "rpcerror.h"
#include "user.pb.h"
#include "zookeeperutil.h"

#include <cstdlib>
#include <iostream>

namespace {
std::string AvatarUrl(int userid)
{
    return "https://api.dicebear.com/7.x/thumbs/svg?seed=" + std::to_string(userid);
}

UserProfile FromPb(const novachat::UserInfo& u)
{
    UserProfile p;
    p.id = u.id();
    p.name = u.name();
    p.state = u.state();
    p.avatar = u.avatar().empty() ? AvatarUrl(u.id()) : u.avatar();
    return p;
}
}  // namespace

UserRpcClient& UserRpcClient::Instance()
{
    static UserRpcClient c;
    return c;
}

bool UserRpcClient::GetUserProfile(int userid, UserProfile* out)
{
    MprpcChannel channel;  // ZK 发现 UserServiceRpc
    MprpcController ctrl;
    novachat::UserServiceRpc_Stub stub(&channel);
    novachat::GetUserProfileRequest req;
    novachat::GetUserProfileResponse resp;
    req.set_userid(userid);
    stub.GetUserProfile(&ctrl, &req, &resp, nullptr);
    if (ctrl.Failed() || resp.result().errcode() != 0) {
        std::cerr << "[UserRPC] GetUserProfile fail uid=" << userid
                  << " err=" << ctrl.ErrorText() << std::endl;
        return false;
    }
    if (out) *out = FromPb(resp.user());
    return true;
}

std::vector<std::string> UserRpcClient::ListZkNodes()
{
    std::vector<std::string> out;
    try {
        ZkClient zk;
        zk.Start();
        const char* path = "/NovaChat-service/UserServiceRpc";
        for (const auto& name : zk.GetChildren(path)) {
            // 只展示实例节点（含 - 或 :），忽略历史遗留的方法目录
            if (name.find('-') == std::string::npos && name.find(':') == std::string::npos)
                continue;
            std::string full = std::string(path) + "/" + name;
            std::string data = zk.GetData(full.c_str());
            out.push_back(data.empty() ? name : data);
        }
    } catch (...) {
    }
    return out;
}
