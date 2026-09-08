#include "mprpcchannel.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "zookeeperutil.h"
#include <string>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done)
{
    (void)controller;
    (void)done;
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name();

    uint32_t args_size = 0;
    std::string args_str;
    if (request->AppendPartialToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        std::cout << "serialize request error!" << std::endl;
        return;
    }

    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_methode_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        std::cout << "serialize rpc header error!" << std::endl;
        return;
    }

    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));
    send_rpc_str += rpc_header_str;
    send_rpc_str += args_str;

    // ZooKeeper 服务发现；失败回退 conf 中的 rpcserverip/port
    std::string ip;
    uint16_t port = 0;
    bool discovered = false;
    {
        ZkClient zkCli;
        zkCli.Start();
        std::string service_path = "/NovaChat-service/" + service_name;
        std::vector<std::string> nodes = zkCli.GetChildren(service_path.c_str());
        if (!nodes.empty()) {
            std::string host_data = nodes[0];  // 简单取第一个；可扩展为随机负载均衡
            auto pos = host_data.find(':');
            if (pos != std::string::npos) {
                ip = host_data.substr(0, pos);
                port = static_cast<uint16_t>(atoi(host_data.substr(pos + 1).c_str()));
                discovered = true;
                std::cout << "[zk] discover " << service_name << " -> " << ip << ":" << port << std::endl;
            }
        }
    }
    if (!discovered) {
        ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
        port = static_cast<uint16_t>(atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str()));
        std::cout << "[zk] fallback config " << ip << ":" << port << std::endl;
    }

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cout << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        std::cout << "connect error! errno: " << errno << std::endl;
        close(clientfd);
        return;
    }

    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        std::cout << "send error! errno: " << errno << std::endl;
        close(clientfd);
        return;
    }

    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        std::cout << "recv error! errno: " << errno << std::endl;
        close(clientfd);
        return;
    }

    std::string response_str(recv_buf, static_cast<size_t>(recv_size));
    if (!response->ParseFromString(response_str))
    {
        std::cout << "parse error! response_str size=" << recv_size << std::endl;
        close(clientfd);
        return;
    }

    close(clientfd);
}
