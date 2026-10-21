#pragma once

#include "chatpush.pb.h"
#include <mutex>
#include <string>
#include <unordered_set>

class ChatPushServiceImpl : public novachat::ChatPushServiceRpc
{
public:
    void PushPrivateMsg(::google::protobuf::RpcController* controller,
                        const novachat::PushPrivateMsgRequest* request,
                        novachat::PushPrivateMsgResponse* response,
                        ::google::protobuf::Closure* done) override;

private:
    bool SeenOrInsert(const std::string& key);

    std::mutex mu_;
    std::unordered_set<std::string> recent_keys_;
};
