#pragma once

#include <string>
#include <vector>

struct PresenceView {
    int userid = 0;
    bool online = false;
    std::string gateway_id;
    int64_t last_seen_ms = 0;
};

// Gateway 侧 Presence RPC 客户端（打到独立 PresenceService 进程）
class PresenceRpcClient
{
public:
    static PresenceRpcClient& Instance();

    bool Register(int userid, const std::string& gateway_id);
    bool Unregister(int userid, const std::string& gateway_id);
    bool Query(int userid, PresenceView* out);
    std::vector<PresenceView> List();

private:
    PresenceRpcClient() = default;
};
