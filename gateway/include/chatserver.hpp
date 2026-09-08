#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;

// 聊天服务器主类
class ChatServer
{
public:
    // 初始化聊天服务器对象
    ChatServer(EventLoop *loop,                 // EventLoop = epoll + 回调分发器 + 线程安全执行器
               const InetAddress &listenAddr,   // 服务器监听（提供服务）的端口
               const string &nameArg);
    // 启动服务
    void start();

private:
    // 上报连接相关信息的回调函数
    void onConnection(const TcpConnectionPtr &conn);
    // 上报读写事件相关信息的回调函数
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buffer,
                   Timestamp receiveTime);

    TcpServer _server;  // 组合muduo库，实现服务器功能的类对象
    EventLoop *_loop;  // 指向循环对象的指针
};

#endif