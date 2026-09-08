#include "chatserver.hpp"
#include "chatservice.hpp"
#include "webportal.hpp"
#include "mprpcapplication.h"
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>
using namespace std;
using namespace muduo;
using namespace muduo::net;

void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGINT, resetHandler);

    // argv: NovaGateway [webRoot] [rpcConf]
    string webRoot = "../web";
    string rpcConf = "conf/gateway.conf";
    if (argc >= 2) webRoot = argv[1];
    if (argc >= 3) rpcConf = argv[2];

    // 初始化 RPC 客户端配置（指向 PresenceService）
    {
        char arg0[] = "NovaGateway";
        char arg1[] = "-i";
        vector<char*> args{arg0, arg1, const_cast<char*>(rpcConf.c_str())};
        MprpcApplication::Init(static_cast<int>(args.size()), args.data());
    }

    ChatService::instance()->reset();
    {
        string gid = MprpcApplication::GetInstance().GetConfig().Load("gateway_id");
        if (gid.empty()) gid = "gw-1";
        ChatService::instance()->setGatewayId(gid);
    }

    EventLoop loop;

    InetAddress chatAddr("0.0.0.0", 6666);
    ChatServer chatServer(&loop, chatAddr, "NovaGateway");
    chatServer.start();

    InetAddress httpAddr("0.0.0.0", 8088);
    InetAddress wsAddr("0.0.0.0", 8089);
    WebPortal portal(&loop, httpAddr, wsAddr, webRoot);
    portal.start();

    cout << "NovaChat ready\n"
         << "  TCP client : 127.0.0.1:6666\n"
         << "  Web UI     : http://127.0.0.1:8088/\n"
         << "  WebSocket  : ws://127.0.0.1:8089/\n"
         << "  Nginx入口  : http://127.0.0.1:8090/  ws://127.0.0.1:8090/ws\n"
         << "  PresenceRPC-> " << MprpcApplication::GetInstance().GetConfig().Load("rpcserverip")
         << ":" << MprpcApplication::GetInstance().GetConfig().Load("rpcserverport") << "\n";

    loop.loop();
    return 0;
}
