#pragma once

#include "zookeeperutil.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include <cstdint>
#include <string>
#include <unordered_map>

class RpcProvider
{
public:
    void NotifyService(google::protobuf::Service* service);
    void Run();

private:
    muduo::net::EventLoop m_eventLoop;
    ZkClient m_zkCli;

    struct ServiceInfo
    {
        google::protobuf::Service* m_service = nullptr;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;
    };
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    void OnConnection(const muduo::net::TcpConnectionPtr&);
    void OnMessage(const muduo::net::TcpConnectionPtr&, muduo::net::Buffer*, muduo::Timestamp);
    void SendResponse(const muduo::net::TcpConnectionPtr&,
                      uint32_t request_id,
                      int errcode,
                      const std::string& errmsg,
                      google::protobuf::Message* response);
};
