#pragma once
#include <google/protobuf/service.h>
#include <memory>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <string>
#include <functional>
#include <google/protobuf/descriptor.h>
#include <unordered_map>
#include "zookeeperutil.h"

// 框架提供的专门发布rpc服务的网络对象类
class RpcProvider
{
public:
    void NotifyService(google::protobuf::Service* servive);
    void Run();

private:
    muduo::net::EventLoop m_eventLoop;
    ZkClient m_zkCli;  // 保持会话，临时节点才不会丢

    struct ServiceInfo
    {
        google::protobuf::Service* m_service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;
    };
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    void OnConnection(const muduo::net::TcpConnectionPtr&);
    void OnMessage(const muduo::net::TcpConnectionPtr&, muduo::net::Buffer*, muduo::Timestamp);
    void SendRpcResponse(const muduo::net::TcpConnectionPtr&, google::protobuf::Message*);
};
