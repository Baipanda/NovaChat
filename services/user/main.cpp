#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "user.pb.h"
#include "usermodel.hpp"

#include <iostream>
#include <string>

using namespace novachat;

namespace {
std::string AvatarUrl(int userid)
{
    return "https://api.dicebear.com/7.x/thumbs/svg?seed=" + std::to_string(userid);
}

void FillUserInfo(UserInfo* info, User& u)
{
    info->set_id(u.getId());
    info->set_name(u.getName());
    info->set_state(u.getState());
    info->set_avatar(AvatarUrl(u.getId()));
}

class UserServiceImpl : public UserServiceRpc
{
public:
    void GetUserProfile(::google::protobuf::RpcController*,
                        const GetUserProfileRequest* req,
                        GetUserProfileResponse* resp,
                        ::google::protobuf::Closure* done) override
    {
        User u = userModel_.query(req->userid());
        if (u.getId() == req->userid()) {
            resp->mutable_result()->set_errcode(0);
            FillUserInfo(resp->mutable_user(), u);
            std::cout << "[UserService] GetUserProfile uid=" << req->userid()
                      << " name=" << u.getName() << std::endl;
        } else {
            resp->mutable_result()->set_errcode(1);
            resp->mutable_result()->set_errmsg("user not found");
        }
        done->Run();
    }

private:
    UserModel userModel_;
};
}  // namespace

int main(int argc, char** argv)
{
    MprpcApplication::Init(argc, argv);
    RpcProvider provider;
    UserServiceImpl service;
    provider.NotifyService(&service);
    std::cout << "[UserService] serving GetUserProfile, ZK=/NovaChat-service/UserServiceRpc\n";
    provider.Run();
    return 0;
}
