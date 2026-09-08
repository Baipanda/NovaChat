#pragma once
#include <muduo/net/TcpConnection.h>
#include <string>
#include <vector>

void ClientSend(const muduo::net::TcpConnectionPtr& conn, const std::string& payload);

bool IsWebSocketConn(const muduo::net::TcpConnectionPtr& conn);
void MarkWebSocketConn(const muduo::net::TcpConnectionPtr& conn);

std::string WsEncodeText(const std::string& payload);
size_t WsDecodeTexts(std::string& buffer, std::vector<std::string>* out_messages);
