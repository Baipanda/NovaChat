#include <iostream>
#include "mprpcapplication.h"

int main(int argc, char** argv)
{
    MprpcApplication::Init(argc, argv);
    std::cout << "[FriendService] NovaChat friend/group RPC node ready (skeleton).\n"
              << "  IDL: proto/friend.proto\n";
    return 0;
}
