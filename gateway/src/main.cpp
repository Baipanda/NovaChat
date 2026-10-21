#include "chatserver.hpp"
#include "chatservice.hpp"
#include "mprpcapplication.h"
#include "webportal.hpp"

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>

using namespace std;
using namespace muduo;
using namespace muduo::net;

void resetHandler(int)
{
    // 多 Gateway 时只下线本机连接用户，避免把另一台机器上的人全打成 offline
    ChatService::instance()->offlineLocalUsers();
    exit(0);
}

static uint16_t ConfPort(const string& key, uint16_t def)
{
    string v = MprpcApplication::GetInstance().GetConfig().Load(key);
    if (v.empty()) return def;
    return static_cast<uint16_t>(atoi(v.c_str()));
}

int main(int argc, char** argv)
{
    signal(SIGINT, resetHandler);

    string webRoot = "../web";
    string rpcConf = "conf/gateway.conf";
    if (argc >= 2) webRoot = argv[1];
    if (argc >= 3) rpcConf = argv[2];

    {
        char arg0[] = "NovaGateway";
        char arg1[] = "-i";
        vector<char*> args{arg0, arg1, const_cast<char*>(rpcConf.c_str())};
        MprpcApplication::Init(static_cast<int>(args.size()), args.data());
    }

    // 注意：启动时不要 reset 全表 online，否则双机部署会互相踢状态
    {
        string gid = MprpcApplication::GetInstance().GetConfig().Load("gateway_id");
        if (gid.empty()) gid = "gw-1";
        ChatService::instance()->setGatewayId(gid);
    }

    uint16_t tcpPort = ConfPort("tcp_port", 6666);
    uint16_t httpPort = ConfPort("http_port", 8088);
    uint16_t wsPort = ConfPort("ws_port", 8089);

    EventLoop loop;
    InetAddress chatAddr("0.0.0.0", tcpPort);
    ChatServer chatServer(&loop, chatAddr, "NovaGateway");
    chatServer.start();

    InetAddress httpAddr("0.0.0.0", httpPort);
    InetAddress wsAddr("0.0.0.0", wsPort);
    WebPortal portal(&loop, httpAddr, wsAddr, webRoot);
    portal.start();

    cout << "NovaChat Gateway [" << ChatService::instance()->gatewayId() << "] ready\n"
         << "  HTTP : http://127.0.0.1:" << httpPort << "/\n"
         << "  WS   : ws://127.0.0.1:" << wsPort << "/\n"
         << "  TCP  : " << tcpPort << "\n"
         << "  Msg  : MySQL persist + Redis Pub/Sub (cross-gateway)\n";

    loop.loop();
    return 0;
}
