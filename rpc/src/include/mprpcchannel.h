#pragma once

#include <cstdint>
#include <google/protobuf/service.h>
#include <string>

class MprpcChannel : public google::protobuf::RpcChannel
{
public:
    MprpcChannel() = default;
    MprpcChannel(std::string ip, uint16_t port);

    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController* controller,
                    const google::protobuf::Message* request,
                    google::protobuf::Message* response,
                    google::protobuf::Closure* done) override;

    int LastError() const { return last_error_; }

private:
    bool fixed_addr_ = false;
    std::string ip_;
    uint16_t port_ = 0;
    int last_error_ = 0;
};
