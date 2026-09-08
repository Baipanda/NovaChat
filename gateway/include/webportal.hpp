#pragma once
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/http/HttpServer.h>
#include <map>
#include <string>

// 浏览器入口：HTTP :8088 页面 + WebSocket :8089 复用 ChatService
class WebPortal
{
public:
    WebPortal(muduo::net::EventLoop* loop,
              const muduo::net::InetAddress& httpAddr,
              const muduo::net::InetAddress& wsAddr,
              std::string webRoot);

    void start();

private:
    void onHttpRequest(const muduo::net::HttpRequest& req, muduo::net::HttpResponse* resp);
    void onWsConnection(const muduo::net::TcpConnectionPtr& conn);
    void onWsMessage(const muduo::net::TcpConnectionPtr& conn,
                     muduo::net::Buffer* buf,
                     muduo::Timestamp time);
    bool tryWsHandshake(const muduo::net::TcpConnectionPtr& conn, std::string* buffer);

    muduo::net::HttpServer _http;
    muduo::net::TcpServer _ws;
    std::string _webRoot;
    std::map<muduo::net::TcpConnectionPtr, std::string> _wsBuffers;
};