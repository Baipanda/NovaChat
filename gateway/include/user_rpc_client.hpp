#pragma once

#include <string>
#include <vector>

struct UserProfile {
    int id = 0;
    std::string name;
    std::string state;
    std::string avatar;
};

// ChatServer -> UserService：ZK 发现服务，RPC 拉取用户资料（昵称/头像等）
class UserRpcClient
{
public:
    static UserRpcClient& Instance();

    bool GetUserProfile(int userid, UserProfile* out);
    // 列出 UserServiceRpc 下实例节点（UI 演示）
    std::vector<std::string> ListZkNodes();
};
