#include "presence.pb.h"
#include "presence_store.hpp"
#include "mprpcapplication.h"
#include "rpcprovider.h"

#include <chrono>
#include <iostream>
#include <string>

using namespace novachat;

namespace {

int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

class PresenceServiceImpl : public PresenceServiceRpc
{
public:
    explicit PresenceServiceImpl(PresenceStore* store) : store_(store) {}

    void Register(::google::protobuf::RpcController*,
                  const RegisterRequest* req,
                  RegisterResponse* resp,
                  ::google::protobuf::Closure* done) override
    {
        store_->Register(req->userid(), req->gateway_id(), NowMs());
        std::cout << "[Presence] Register uid=" << req->userid()
                  << " gw=" << req->gateway_id() << std::endl;
        resp->mutable_result()->set_errcode(0);
        resp->mutable_result()->set_errmsg("");
        done->Run();
    }

    void Unregister(::google::protobuf::RpcController*,
                    const UnregisterRequest* req,
                    UnregisterResponse* resp,
                    ::google::protobuf::Closure* done) override
    {
        store_->Unregister(req->userid(), req->gateway_id());
        std::cout << "[Presence] Unregister uid=" << req->userid()
                  << " gw=" << req->gateway_id() << std::endl;
        resp->mutable_result()->set_errcode(0);
        done->Run();
    }

    void Query(::google::protobuf::RpcController*,
               const QueryRequest* req,
               QueryResponse* resp,
               ::google::protobuf::Closure* done) override
    {
        PresenceRecord rec;
        bool found = store_->Query(req->userid(), &rec);
        resp->mutable_result()->set_errcode(0);
        PresenceInfo* info = resp->mutable_info();
        info->set_userid(req->userid());
        if (found) {
            info->set_online(rec.online);
            info->set_gateway_id(rec.gateway_id);
            info->set_last_seen_ms(rec.last_seen_ms);
        } else {
            info->set_online(false);
            info->set_gateway_id("");
            info->set_last_seen_ms(0);
        }
        done->Run();
    }

    void Kick(::google::protobuf::RpcController*,
              const KickRequest* req,
              KickResponse* resp,
              ::google::protobuf::Closure* done) override
    {
        PresenceRecord prev;
        bool ok = store_->Kick(req->userid(), &prev);
        resp->mutable_result()->set_errcode(ok ? 0 : 1);
        if (ok) resp->set_gateway_id(prev.gateway_id);
        done->Run();
    }

    void List(::google::protobuf::RpcController*,
              const ListRequest*,
              ListResponse* resp,
              ::google::protobuf::Closure* done) override
    {
        // PresenceStore 没有 List API 时用内部 map：扩展 store
        resp->mutable_result()->set_errcode(0);
        for (const auto& kv : store_->Snapshot()) {
            PresenceInfo* info = resp->add_list();
            info->set_userid(kv.first);
            info->set_online(kv.second.online);
            info->set_gateway_id(kv.second.gateway_id);
            info->set_last_seen_ms(kv.second.last_seen_ms);
        }
        done->Run();
    }

private:
    PresenceStore* store_;
};

}  // namespace

int main(int argc, char** argv)
{
    MprpcApplication::Init(argc, argv);

    PresenceStore store;
    PresenceServiceImpl service(&store);

    RpcProvider provider;
    provider.NotifyService(&service);
    std::cout << "[PresenceService] RPC node starting..." << std::endl;
    provider.Run();
    return 0;
}
