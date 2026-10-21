#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcerror.h"
#include "rpcframe.h"
#include "rpcheader.pb.h"

#include <functional>
#include <iostream>

namespace {
class FunctionClosure : public google::protobuf::Closure
{
public:
    explicit FunctionClosure(std::function<void()> fn) : fn_(std::move(fn)) {}
    void Run() override
    {
        fn_();
        delete this;
    }

private:
    std::function<void()> fn_;
};
}  // namespace

void RpcProvider::NotifyService(google::protobuf::Service* service)
{
    ServiceInfo info;
    auto* desc = service->GetDescriptor();
    std::cout << "service_name: " << desc->name() << std::endl;
    for (int i = 0; i < desc->method_count(); ++i) {
        auto* m = desc->method(i);
        info.m_methodMap[m->name()] = m;
        std::cout << "  method: " << m->name() << std::endl;
    }
    info.m_service = service;
    m_serviceMap[desc->name()] = info;
}

void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        std::cout << "[rpc] peer connected " << conn->peerAddress().toIpPort() << std::endl;
    } else {
        std::cout << "[rpc] peer disconnected " << conn->peerAddress().toIpPort() << std::endl;
    }
}

void RpcProvider::SendResponse(const muduo::net::TcpConnectionPtr& conn,
                               uint32_t request_id,
                               int errcode,
                               const std::string& errmsg,
                               google::protobuf::Message* response)
{
    std::string body;
    if (response && errcode == RpcErr::kOk) {
        if (!response->SerializeToString(&body)) {
            errcode = RpcErr::kSerializeFail;
            body.clear();
        }
    }
    mprpc::RpcHeader header;
    header.set_msg_type(RpcMsg::kResponse);
    header.set_request_id(request_id);
    header.set_args_size(static_cast<uint32_t>(body.size()));
    header.set_errcode(errcode);
    header.set_errmsg(errmsg);
    conn->send(RpcFrame::Encode(header, body));
    delete response;
}

void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn,
                            muduo::net::Buffer* buffer,
                            muduo::Timestamp)
{
    for (;;) {
        mprpc::RpcHeader header;
        std::string body;
        size_t consumed = 0;
        int derr = 0;
        bool ok = RpcFrame::TryDecode(buffer->peek(), buffer->readableBytes(), &consumed, &header,
                                      &body, &derr);
        if (!ok) {
            if (derr != 0) {
                std::cerr << "[rpc] bad frame, close peer" << std::endl;
                conn->shutdown();
            }
            return;
        }
        buffer->retrieve(consumed);

        if (header.msg_type() == RpcMsg::kPing) {
            mprpc::RpcHeader ack;
            ack.set_msg_type(RpcMsg::kAck);
            ack.set_request_id(header.request_id());
            ack.set_args_size(0);
            ack.set_errcode(0);
            conn->send(RpcFrame::Encode(ack, ""));
            continue;
        }
        if (header.msg_type() != RpcMsg::kRequest && header.msg_type() != 0) {
            continue;
        }

        const uint32_t request_id = header.request_id();
        const std::string service_name = header.service_name();
        const std::string method_name = header.methode_name();

        auto it = m_serviceMap.find(service_name);
        if (it == m_serviceMap.end()) {
            SendResponse(conn, request_id, RpcErr::kServiceNotFound, service_name, nullptr);
            continue;
        }
        auto mit = it->second.m_methodMap.find(method_name);
        if (mit == it->second.m_methodMap.end()) {
            SendResponse(conn, request_id, RpcErr::kMethodNotFound, method_name, nullptr);
            continue;
        }

        auto* service = it->second.m_service;
        auto* method = mit->second;
        auto* request = service->GetRequestPrototype(method).New();
        if (!request->ParseFromString(body)) {
            delete request;
            SendResponse(conn, request_id, RpcErr::kInvalidRequest, "parse request", nullptr);
            continue;
        }
        auto* response = service->GetResponsePrototype(method).New();
        auto* done = new FunctionClosure([this, conn, response, request_id, request]() {
            SendResponse(conn, request_id, RpcErr::kOk, "", response);
            delete request;
        });
        service->CallMethod(method, nullptr, request, response, done);
    }
}

void RpcProvider::Run()
{
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = static_cast<uint16_t>(
        atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str()));
    muduo::net::InetAddress address(ip, port);

    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");
    server.setConnectionCallback(
        std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3));

    std::string advertise = MprpcApplication::GetInstance().GetConfig().Load("rpc_advertise_ip");
    if (advertise.empty()) advertise = (ip == "0.0.0.0") ? "127.0.0.1" : ip;

    m_zkCli.Start();
    m_zkCli.Create("/NovaChat-service", nullptr, 0);
    for (auto& sp : m_serviceMap) {
        // 只注册到服务级：/NovaChat-service/UserServiceRpc/127.0.0.1-8001
        // 方法名在 RPC 请求头里，连上服务后再本地分发
        std::string service_path = "/NovaChat-service/" + sp.first;
        m_zkCli.Create(service_path.c_str(), nullptr, 0);

        std::string host = advertise + ":" + std::to_string(port);
        std::string node_name = advertise + "-" + std::to_string(port);
        std::string node_path = service_path + "/" + node_name;
        m_zkCli.Create(node_path.c_str(), host.c_str(), static_cast<int>(host.size()),
                       ZOO_EPHEMERAL);
        std::cout << "[zk] register " << sp.first << " -> " << host
                  << " path=" << node_path << std::endl;
    }

    std::cout << "RpcProvider start ip=" << ip << " port=" << port
              << " advertise=" << advertise << std::endl;
    server.setThreadNum(4);
    server.start();
    m_eventLoop.loop();
}
