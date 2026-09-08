#include <iostream>
#include "mprpcapplication.h"
#include "rpcprovider.h"

/*
 * NovaChat UserService
 *
 * 目标形态（伪代码）：
 *   class UserService : public novachat::UserServiceRpc {
 *     void Login(..., const LoginRequest* req, LoginResponse* resp, Closure* done) {
 *       // UserModel::query / updateState
 *       // OfflineMsgModel::query
 *       // FriendModel::query
 *       done->Run();
 *     }
 *   };
 *
 *   RpcProvider provider;
 *   provider.NotifyService(new UserService);
 *   provider.Run();
 */

int main(int argc, char** argv)
{
    MprpcApplication::Init(argc, argv);

    std::cout << "[UserService] NovaChat user RPC node ready (skeleton).\n"
              << "  Next: bind novachat::UserServiceRpc + MySQL models.\n"
              << "  Config: " << (argc > 2 ? argv[2] : "conf/user.conf") << std::endl;

    // 骨架阶段：初始化框架后直接退出说明，避免无服务注册时空转。
    // 接入 proto 后改为 RpcProvider::Run()。
    return 0;
}
