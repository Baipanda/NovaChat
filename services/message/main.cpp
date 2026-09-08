#include <iostream>
#include "mprpcapplication.h"

int main(int argc, char** argv)
{
    MprpcApplication::Init(argc, argv);
    std::cout << "[MessageService] NovaChat message/offline RPC node ready (skeleton).\n"
              << "  IDL: proto/message.proto\n";
    return 0;
}
