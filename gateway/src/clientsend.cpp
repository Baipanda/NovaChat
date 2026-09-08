#include "clientsend.hpp"
#include <boost/any.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

using muduo::net::TcpConnectionPtr;

static const char* kWsTag = "ws";

bool IsWebSocketConn(const TcpConnectionPtr& conn)
{
    if (!conn) return false;
    const boost::any& ctx = conn->getContext();
    if (ctx.empty()) return false;
    try {
        return boost::any_cast<std::string>(ctx) == kWsTag;
    } catch (...) {
        return false;
    }
}

void MarkWebSocketConn(const TcpConnectionPtr& conn)
{
    if (conn) conn->setContext(std::string(kWsTag));
}

std::string WsEncodeText(const std::string& payload)
{
    std::string out;
    const size_t n = payload.size();
    out.push_back(static_cast<char>(0x81)); // FIN + text
    if (n < 126) {
        out.push_back(static_cast<char>(n));
    } else if (n <= 0xffff) {
        out.push_back(126);
        out.push_back(static_cast<char>((n >> 8) & 0xff));
        out.push_back(static_cast<char>(n & 0xff));
    } else {
        out.push_back(127);
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>((n >> (8 * i)) & 0xff));
        }
    }
    out.append(payload);
    return out;
}

size_t WsDecodeTexts(std::string& buffer, std::vector<std::string>* out_messages)
{
    size_t consumed = 0;
    while (true) {
        if (buffer.size() < 2) break;
        const unsigned char* data = reinterpret_cast<const unsigned char*>(buffer.data());
        bool fin = (data[0] & 0x80) != 0;
        int opcode = data[0] & 0x0f;
        bool masked = (data[1] & 0x80) != 0;
        uint64_t len = data[1] & 0x7f;
        size_t header = 2;
        if (len == 126) {
            if (buffer.size() < 4) break;
            len = (uint64_t(data[2]) << 8) | data[3];
            header = 4;
        } else if (len == 127) {
            if (buffer.size() < 10) break;
            len = 0;
            for (int i = 0; i < 8; ++i) len = (len << 8) | data[2 + i];
            header = 10;
        }
        size_t maskOff = header;
        if (masked) header += 4;
        if (buffer.size() < header + len) break;

        std::string payload = buffer.substr(header, static_cast<size_t>(len));
        if (masked) {
            unsigned char mask[4];
            memcpy(mask, buffer.data() + maskOff, 4);
            for (size_t i = 0; i < payload.size(); ++i) {
                payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
            }
        }

        size_t frameSize = header + static_cast<size_t>(len);
        buffer.erase(0, frameSize);
        consumed += frameSize;

        if (opcode == 0x8) { // close
            break;
        }
        if (opcode == 0x9) { // ping -> ignore / could pong
            continue;
        }
        if ((opcode == 0x1 || opcode == 0x0) && fin && out_messages) {
            out_messages->push_back(payload);
        }
    }
    return consumed;
}

void ClientSend(const TcpConnectionPtr& conn, const std::string& payload)
{
    if (!conn || !conn->connected()) return;
    if (IsWebSocketConn(conn)) {
        conn->send(WsEncodeText(payload));
    } else {
        conn->send(payload);
    }
}
