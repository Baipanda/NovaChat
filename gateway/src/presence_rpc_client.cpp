#include "presence_rpc_client.hpp"
#include "mprpcchannel.h"
#include "presence.pb.h"

#include <iostream>
#include <memory>

PresenceRpcClient& PresenceRpcClient::Instance()
{
    static PresenceRpcClient c;
    return c;
}

bool PresenceRpcClient::Register(int userid, const std::string& gateway_id)
{
    try {
        MprpcChannel channel;
        novachat::PresenceServiceRpc_Stub stub(&channel);
        novachat::RegisterRequest req;
        novachat::RegisterResponse resp;
        req.set_userid(userid);
        req.set_gateway_id(gateway_id);
        stub.Register(nullptr, &req, &resp, nullptr);
        return resp.result().errcode() == 0;
    } catch (...) {
        std::cerr << "[PresenceRpc] Register exception\n";
        return false;
    }
}

bool PresenceRpcClient::Unregister(int userid, const std::string& gateway_id)
{
    try {
        MprpcChannel channel;
        novachat::PresenceServiceRpc_Stub stub(&channel);
        novachat::UnregisterRequest req;
        novachat::UnregisterResponse resp;
        req.set_userid(userid);
        req.set_gateway_id(gateway_id);
        stub.Unregister(nullptr, &req, &resp, nullptr);
        return resp.result().errcode() == 0;
    } catch (...) {
        std::cerr << "[PresenceRpc] Unregister exception\n";
        return false;
    }
}

bool PresenceRpcClient::Query(int userid, PresenceView* out)
{
    try {
        MprpcChannel channel;
        novachat::PresenceServiceRpc_Stub stub(&channel);
        novachat::QueryRequest req;
        novachat::QueryResponse resp;
        req.set_userid(userid);
        stub.Query(nullptr, &req, &resp, nullptr);
        if (out) {
            out->userid = userid;
            out->online = resp.info().online();
            out->gateway_id = resp.info().gateway_id();
            out->last_seen_ms = resp.info().last_seen_ms();
        }
        return resp.result().errcode() == 0;
    } catch (...) {
        std::cerr << "[PresenceRpc] Query exception\n";
        return false;
    }
}

std::vector<PresenceView> PresenceRpcClient::List()
{
    std::vector<PresenceView> out;
    try {
        MprpcChannel channel;
        novachat::PresenceServiceRpc_Stub stub(&channel);
        novachat::ListRequest req;
        novachat::ListResponse resp;
        stub.List(nullptr, &req, &resp, nullptr);
        for (int i = 0; i < resp.list_size(); ++i) {
            const auto& info = resp.list(i);
            PresenceView v;
            v.userid = info.userid();
            v.online = info.online();
            v.gateway_id = info.gateway_id();
            v.last_seen_ms = info.last_seen_ms();
            out.push_back(v);
        }
    } catch (...) {
        std::cerr << "[PresenceRpc] List exception\n";
    }
    return out;
}
