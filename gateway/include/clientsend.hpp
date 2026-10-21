#pragma once
#include <muduo/net/TcpConnection.h>
#include <string>
#include <vector>

struct ClientConnCtx
{
    bool websocket = false;
    int userid = -1;
};

void ClientSend(const muduo::net::TcpConnectionPtr& conn, const std::string& payload);

bool IsWebSocketConn(const muduo::net::TcpConnectionPtr& conn);
void MarkWebSocketConn(const muduo::net::TcpConnectionPtr& conn);
void BindConnUser(const muduo::net::TcpConnectionPtr& conn, int userid);
int BoundUserId(const muduo::net::TcpConnectionPtr& conn);

std::string WsEncodeText(const std::string& payload);
// got_close: 收到 WebSocket Close 帧时置 true
size_t WsDecodeTexts(std::string& buffer, std::vector<std::string>* out_messages,
                     bool* got_close = nullptr);
