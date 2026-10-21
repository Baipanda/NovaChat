#include "webportal.hpp"
#include "chatservice.hpp"
#include "clientsend.hpp"
#include "json.hpp"
#include "user_rpc_client.hpp"

#include <muduo/base/Logging.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpResponse.h>

#include <fstream>
#include <sstream>
#include <functional>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;
using placeholders::_1;
using placeholders::_2;
using placeholders::_3;

namespace {

string ReadFile(const string& path)
{
    ifstream ifs(path.c_str(), ios::in | ios::binary);
    if (!ifs) return string();
    ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

string Base64Encode(const unsigned char* data, size_t len)
{
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);
    BUF_MEM* bptr = nullptr;
    BIO_get_mem_ptr(b64, &bptr);
    string out(bptr->data, bptr->length);
    BIO_free_all(b64);
    return out;
}

string WsAcceptKey(const string& clientKey)
{
    string src = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(src.data()), src.size(), hash);
    return Base64Encode(hash, SHA_DIGEST_LENGTH);
}

string GuessContentType(const string& path)
{
    if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") return "application/javascript; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") return "text/css; charset=utf-8";
    return "text/plain; charset=utf-8";
}

}  // namespace

WebPortal::WebPortal(EventLoop* loop,
                     const InetAddress& httpAddr,
                     const InetAddress& wsAddr,
                     string webRoot)
    : _http(loop, httpAddr, "NovaHttp"),
      _ws(loop, wsAddr, "NovaWs"),
      _webRoot(std::move(webRoot))
{
    _http.setHttpCallback(bind(&WebPortal::onHttpRequest, this, _1, _2));
    _ws.setConnectionCallback(bind(&WebPortal::onWsConnection, this, _1));
    _ws.setMessageCallback(bind(&WebPortal::onWsMessage, this, _1, _2, _3));
}

void WebPortal::start()
{
    _http.start();
    _ws.start();
    LOG_INFO << "Web UI http://127.0.0.1:8088/  ws://127.0.0.1:8089/";
}

void WebPortal::onHttpRequest(const HttpRequest& req, HttpResponse* resp)
{
    string path = req.path();
    if (path == "/") path = "/index.html";

    // ZK 演示：列出 UserServiceRpc 临时节点
    if (path == "/api/zk/userservice") {
        json j;
        j["path"] = "/NovaChat-service/UserServiceRpc";
        j["nodes"] = UserRpcClient::Instance().ListZkNodes();
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json; charset=utf-8");
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->setBody(j.dump());
        return;
    }

    if (path.find("..") != string::npos) {
        resp->setStatusCode(HttpResponse::k400BadRequest);
        resp->setStatusMessage("Bad Request");
        resp->setCloseConnection(true);
        return;
    }

    string filePath = _webRoot + path;
    string body = ReadFile(filePath);
    if (body.empty()) {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setCloseConnection(true);
        resp->setBody("Not Found");
        return;
    }

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType(GuessContentType(path));
    resp->addHeader("Cache-Control", "no-cache");
    resp->setBody(body);
}

void WebPortal::onWsConnection(const TcpConnectionPtr& conn)
{
    if (!conn->connected()) {
        ChatService::instance()->clientCloseException(conn);
        _wsBuffers.erase(conn);
        conn->shutdown();
    } else {
        _wsBuffers[conn].clear();
    }
}

bool WebPortal::tryWsHandshake(const TcpConnectionPtr& conn, string* buffer)
{
    size_t pos = buffer->find("\r\n\r\n");
    if (pos == string::npos) return false;

    string header = buffer->substr(0, pos);
    buffer->erase(0, pos + 4);

    string key;
    istringstream iss(header);
    string line;
    getline(iss, line); // request line
    while (getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const string prefix = "Sec-WebSocket-Key:";
        if (line.size() > prefix.size() && line.compare(0, prefix.size(), prefix) == 0) {
            key = line.substr(prefix.size());
            while (!key.empty() && key[0] == ' ') key.erase(0, 1);
        }
    }
    if (key.empty()) {
        conn->shutdown();
        return true;
    }

    string accept = WsAcceptKey(key);
    ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept << "\r\n"
        << "\r\n";
    conn->send(oss.str());
    MarkWebSocketConn(conn);
    LOG_INFO << "WebSocket handshake ok " << conn->peerAddress().toIpPort();
    return true;
}

void WebPortal::onWsMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
    string& cache = _wsBuffers[conn];
    cache.append(buf->retrieveAllAsString());

    if (!IsWebSocketConn(conn)) {
        if (!tryWsHandshake(conn, &cache)) return;
        if (cache.empty()) return;
    }

    vector<string> messages;
    bool got_close = false;
    WsDecodeTexts(cache, &messages, &got_close);
    for (string& msg : messages) {
        // 兼容前端 JSON；去掉可能的尾零
        while (!msg.empty() && msg.back() == '\0') msg.pop_back();
        if (msg.empty()) continue;
        try {
            json js = json::parse(msg);
            auto handler = ChatService::instance()->getHandler(js["msgid"].get<int>());
            handler(conn, js, time);
        } catch (const exception& ex) {
            LOG_ERROR << "WS json error: " << ex.what() << " raw=" << msg;
            ClientSend(conn, string("{\"type\":\"system\",\"msg\":\"bad json\"}"));
        }
    }
    // 浏览器关页常先发 Close 帧；立刻落 offline，不等 TCP 完全断开
    if (got_close) {
        ChatService::instance()->clientCloseException(conn);
        _wsBuffers.erase(conn);
        if (conn->connected()) conn->shutdown();
    }
}
