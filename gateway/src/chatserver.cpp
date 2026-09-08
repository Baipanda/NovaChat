#include "chatserver.hpp"
#include "chatservice.hpp"
#include <functional>
#include <string>
// vscode中的跳转，是c_cpp_properties.json定义的
// 包括using namespace muduo::net;
// 如果里面没有/usr/include，会导致无法识别EventLoop
#include "json.hpp"
using namespace std;
using namespace placeholders;
using json = nlohmann::json;
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg)
{
    // 注册连接回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开连接
    if(!conn->connected()){
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// muduo库在onMessage这里只负责把缓冲区内容交付，至于反序列化，反序列化之后如何处理，由用户自己实现
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据反序列化
    json js = json::parse(buf);
    // 通过js["msgid"] 获取一个业务处理器handler
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的处理器来执行相应的业务处理
    msgHandler(conn, js, time);
}