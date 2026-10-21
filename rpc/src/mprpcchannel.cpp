#include "mprpcchannel.h"
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "rpcconn.h"
#include "rpcerror.h"
#include "rpcframe.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"

#include <atomic>
#include <cstdlib>
#include <iostream>

namespace {
constexpr int kDefaultTimeoutMs = 3000;
std::atomic<uint32_t> g_request_id{1};

uint32_t NextRequestId()
{
    uint32_t id = g_request_id.fetch_add(1);
    if (id == 0) id = g_request_id.fetch_add(1);
    return id;
}

void SetCtrl(google::protobuf::RpcController* controller, int code)
{
    if (!controller) return;
    if (auto* c = dynamic_cast<MprpcController*>(controller)) {
        c->SetError(code);
    } else if (code != RpcErr::kOk) {
        controller->SetFailed(RpcErr::ToString(code));
    }
}
}  // namespace

MprpcChannel::MprpcChannel(std::string ip, uint16_t port)
    : fixed_addr_(true), ip_(std::move(ip)), port_(port)
{
}

void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                              google::protobuf::RpcController* controller,
                              const google::protobuf::Message* request,
                              google::protobuf::Message* response,
                              google::protobuf::Closure* done)
{
    (void)done;
    last_error_ = RpcErr::kOk;
    if (controller) controller->Reset();

    std::string args_str;
    if (!request->SerializeToString(&args_str)) {
        last_error_ = RpcErr::kSerializeFail;
        SetCtrl(controller, last_error_);
        return;
    }
    if (args_str.size() > RpcFrame::kMaxBodyLen) {
        last_error_ = RpcErr::kInvalidRequest;
        SetCtrl(controller, last_error_);
        return;
    }

    const uint32_t request_id = NextRequestId();
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(method->service()->name());
    rpcHeader.set_methode_name(method->name());
    rpcHeader.set_args_size(static_cast<uint32_t>(args_str.size()));
    rpcHeader.set_request_id(request_id);
    rpcHeader.set_msg_type(RpcMsg::kRequest);
    rpcHeader.set_errcode(0);

    std::string frame = RpcFrame::Encode(rpcHeader, args_str);

    std::string ip = ip_;
    uint16_t port = port_;
    if (!fixed_addr_) {
        bool discovered = false;
        ZkClient zkCli;
        zkCli.Start();
        // 服务级发现：/NovaChat-service/{ServiceName}/{ip-port}
        std::string service_path = "/NovaChat-service/" + std::string(method->service()->name());
        for (const auto& name : zkCli.GetChildren(service_path.c_str())) {
            if (name.find('-') == std::string::npos && name.find(':') == std::string::npos)
                continue;  // 跳过历史遗留的方法目录节点
            std::string full = service_path + "/" + name;
            std::string addr = zkCli.GetData(full.c_str());
            if (addr.empty()) addr = name;
            auto pos = addr.find(':');
            if (pos == std::string::npos) {
                pos = addr.rfind('-');
                if (pos != std::string::npos) addr[pos] = ':';
                pos = addr.find(':');
            }
            if (pos != std::string::npos) {
                ip = addr.substr(0, pos);
                port = static_cast<uint16_t>(atoi(addr.substr(pos + 1).c_str()));
                if (!ip.empty() && port != 0) {
                    discovered = true;
                    break;
                }
            }
        }
        if (!discovered) {
            ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
            port = static_cast<uint16_t>(
                atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str()));
        }
    }

    auto conn = RpcLongConnection::Get(ip, port);
    std::string response_str;
    int header_errcode = 0;
    int rc = conn->Call(frame, request_id, &response_str, &header_errcode, kDefaultTimeoutMs);
    last_error_ = rc;
    SetCtrl(controller, rc);
    if (rc != RpcErr::kOk) {
        std::cerr << "[rpc] " << method->service()->name() << "." << method->name()
                  << " id=" << request_id << " err=" << RpcErr::ToString(rc) << std::endl;
        return;
    }
    if (!response->ParseFromString(response_str)) {
        last_error_ = RpcErr::kParseFail;
        SetCtrl(controller, last_error_);
    }
}
