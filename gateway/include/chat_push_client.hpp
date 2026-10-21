#pragma once

#include <string>

struct ChatPushResult {
    int rpc_err = 0;       // RpcErr::* transport
    int business_err = -1; // body result.errcode when rpc_err==0
    bool duplicated = false;
};

class ChatPushClient
{
public:
    // Push with idempotency_key. On TIMEOUT, caller may retry with the SAME key.
    static ChatPushResult PushPrivateMsg(const std::string& rpc_addr, int userid,
                                         const std::string& payload,
                                         const std::string& idempotency_key);
};
