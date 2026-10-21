#pragma once

#include "redis.hpp"
#include <string>
#include <vector>

using PresenceView = PresenceRecord;

class PresenceClient
{
public:
    static PresenceClient& Instance();
    void BindRedis(Redis* redis);
    bool Register(int userid, const std::string& gateway_id, const std::string& rpc_addr);
    bool Unregister(int userid);
    bool Refresh(int userid);
    bool Query(int userid, PresenceView* out);
    std::vector<PresenceView> List();

private:
    PresenceClient() = default;
    Redis* redis_ = nullptr;
    int ttl_sec_ = 90;
};

using PresenceRpcClient = PresenceClient;
