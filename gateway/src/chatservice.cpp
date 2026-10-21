#include "chatservice.hpp"
#include "clientsend.hpp"
#include "public.hpp"
#include "user_rpc_client.hpp"
#include "groupuser.hpp"

#include <muduo/base/Logging.h>
#include <ctime>
#include <vector>

using namespace std;
using namespace muduo;

ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({DEL_FRIEND_MSG, bind(&ChatService::delFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({QUIT_GROUP_MSG, bind(&ChatService::quitGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({OFFLINE_MSG_ACK, bind(&ChatService::offlineAck, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOAD_HISTORY_MSG, bind(&ChatService::loadHistory, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOAD_GROUP_HISTORY_MSG, bind(&ChatService::loadGroupHistory, this, _1, _2, _3)});

    _msgModel.ensureTable();
    if (_redis.connect()) {
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

string ChatService::avatarOf(int userid)
{
    return "https://api.dicebear.com/7.x/thumbs/svg?seed=" + to_string(userid);
}

json ChatService::groupToJson(Group& g)
{
    json grpjson;
    grpjson["id"] = g.getId();
    grpjson["groupname"] = g.getName();
    grpjson["groupdesc"] = g.getDesc();
    // 群头像：与用户头像同源 dicebear，seed 固定为 g{id}
    grpjson["avatar"] = "https://api.dicebear.com/7.x/shapes/svg?seed=g" + to_string(g.getId());
    json users = json::array();
    for (GroupUser& u : g.getUsers()) {
        json uj;
        uj["id"] = u.getId();
        uj["name"] = u.getName();
        uj["state"] = u.getState();
        uj["role"] = u.getRole();
        uj["avatar"] = avatarOf(u.getId());
        users.push_back(uj);
    }
    grpjson["users"] = users;
    grpjson["member_count"] = static_cast<int>(g.getUsers().size());
    return grpjson;
}

vector<string> ChatService::groupsPayload(int userid)
{
    vector<string> groupV;
    for (Group& g : _groupModel.queryGroups(userid)) {
        groupV.push_back(groupToJson(g).dump());
    }
    return groupV;
}

void ChatService::reset() { _userModel.resetState(); }

void ChatService::offlineLocalUsers()
{
    vector<int> ids;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto& p : _userConnMap) ids.push_back(p.first);
        _userConnMap.clear();
    }
    for (int id : ids) {
        User u(id, "", "", "offline");
        _userModel.updateState(u);
        _redis.unsubscribe(id);
        notifyPresence(id, false);
    }
}

void ChatService::notifyPresence(int userid, bool online)
{
    json js;
    js["type"] = "presence";
    js["userid"] = userid;
    js["online"] = online;
    string payload = js.dump();

    // 本机所有在线连接立刻刷新好友绿点
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto& p : _userConnMap) {
            if (p.first == userid) continue;
            ClientSend(p.second, payload);
        }
    }

    // 跨机：推到好友的 userid 频道（对端 Gateway subscribe 后转发）
    for (auto& fr : _friendModel.query(userid)) {
        bool local = false;
        {
            lock_guard<mutex> lock(_connMutex);
            local = _userConnMap.count(fr.getId()) > 0;
        }
        if (!local) _redis.publish(fr.getId(), payload);
    }
}

void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if (user.getId() != id || user.getPwd() != pwd) {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        ClientSend(conn, response.dump());
        return;
    }
    // 本机已登录同账号：顶号；DB 为 online 但本机无连接（关页未落库）允许重连纠正
    TcpConnectionPtr oldConn;
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end() && it->second != conn) {
            oldConn = it->second;
            _userConnMap.erase(it);
        }
    }
    if (oldConn && oldConn->connected()) oldConn->shutdown();

    {
        lock_guard<mutex> lock(_connMutex);
        _userConnMap[id] = conn;
    }
    BindConnUser(conn, id);
    _redis.subscribe(id);
    user.setState("online");
    _userModel.updateState(user);

    UserProfile self;
    string selfAvatar = avatarOf(id);
    if (UserRpcClient::Instance().GetUserProfile(id, &self)) {
        if (!self.name.empty()) user.setName(self.name);
        if (!self.avatar.empty()) selfAvatar = self.avatar;
    }

    json response;
    response["msgid"] = LOGIN_MSG_ACK;
    response["errno"] = 0;
    response["id"] = user.getId();
    response["name"] = user.getName();
    response["avatar"] = selfAvatar;
    response["gateway_id"] = _gatewayId;

    // 好友关系本地查；每人资料走 GetUserProfile（RPC）
    vector<string> vec2;
    for (auto& fr : _friendModel.query(id)) {
        json fjs;
        UserProfile p;
        if (UserRpcClient::Instance().GetUserProfile(fr.getId(), &p)) {
            fjs["id"] = p.id;
            fjs["name"] = p.name;
            fjs["state"] = p.state;
            fjs["avatar"] = p.avatar.empty() ? avatarOf(p.id) : p.avatar;
        } else {
            fjs["id"] = fr.getId();
            fjs["name"] = fr.getName();
            fjs["state"] = fr.getState();
            fjs["avatar"] = avatarOf(fr.getId());
        }
        vec2.push_back(fjs.dump());
    }
    response["friends"] = vec2;
    response["friend"] = vec2;

    // 未读离线消息（status=0），只放在 LOGIN_ACK 里，避免 ACK 前单独推包被前端丢弃
    vector<ChatMessage> pending = _msgModel.queryPending(id);
    if (!pending.empty()) {
        vector<json> arr;
        for (auto& m : pending) {
            json item;
            item["msg_id"] = m.id;
            try {
                item["body"] = json::parse(m.message);
            } catch (...) {
                item["body"] = m.message;
            }
            arr.push_back(item);
        }
        response["offlinemsg"] = arr;
    }

    vector<string> groupV = groupsPayload(id);
    if (!groupV.empty()) {
        response["groups"] = groupV;
    }

    response["zk_nodes"] = UserRpcClient::Instance().ListZkNodes();
    response["zk_path"] = "/NovaChat-service/UserServiceRpc";

    ClientSend(conn, response.dump());
    notifyPresence(id, true);
}

void ChatService::offlineAck(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    (void)conn;
    vector<long long> ids;
    if (js.contains("msg_ids") && js["msg_ids"].is_array()) {
        for (auto& x : js["msg_ids"]) ids.push_back(x.get<long long>());
    } else if (js.contains("msg_id")) {
        ids.push_back(js["msg_id"].get<long long>());
    }
    _msgModel.markDelivered(ids);
}

void ChatService::loadHistory(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int peerid = 0;
    if (js.contains("peerid")) peerid = js["peerid"].get<int>();
    else if (js.contains("toid")) peerid = js["toid"].get<int>();
    int limit = js.contains("limit") ? js["limit"].get<int>() : 50;

    json response;
    response["msgid"] = LOAD_HISTORY_MSG_ACK;
    response["errno"] = 0;
    response["peerid"] = peerid;
    vector<json> arr;
    for (auto& m : _msgModel.queryHistory(userid, peerid, limit)) {
        json item;
        item["msg_id"] = m.id;
        item["fromid"] = m.fromid;
        item["toid"] = m.toid;
        item["time"] = m.createtime;
        try {
            item["body"] = json::parse(m.message);
        } catch (...) {
            item["body"] = m.message;
        }
        arr.push_back(item);
    }
    response["list"] = arr;
    ClientSend(conn, response.dump());
}

void ChatService::loadGroupHistory(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int groupid = 0;
    if (js.contains("groupid")) groupid = js["groupid"].get<int>();
    int limit = js.contains("limit") ? js["limit"].get<int>() : 50;

    json response;
    response["msgid"] = LOAD_GROUP_HISTORY_MSG_ACK;
    response["errno"] = 0;
    response["groupid"] = groupid;
    vector<json> arr;
    for (auto& m : _msgModel.queryGroupHistory(groupid, limit)) {
        json item;
        item["msg_id"] = m.id;
        item["fromid"] = m.fromid;
        item["groupid"] = m.groupid;
        item["time"] = m.createtime;
        try {
            item["body"] = json::parse(m.message);
        } catch (...) {
            item["body"] = m.message;
        }
        arr.push_back(item);
    }
    response["list"] = arr;
    ClientSend(conn, response.dump());
}

void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    User user;
    user.setName(js["name"]);
    user.setPwd(js["password"]);
    bool ok = _userModel.insert(user);
    json response;
    response["msgid"] = REG_MSG_ACK;
    response["errno"] = ok ? 0 : 1;
    if (ok) response["id"] = user.getId();
    ClientSend(conn, response.dump());
}

MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        return [=](const TcpConnectionPtr&, json&, Timestamp) {
            LOG_ERROR << "msgid " << msgid << " can't find handler";
        };
    }
    return it->second;
}

void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int toid = 0;
    if (js.contains("to")) toid = js["to"].get<int>();
    else if (js.contains("toid")) toid = js["toid"].get<int>();
    else {
        json err;
        err["type"] = "system";
        err["msg"] = "chat missing to/toid";
        ClientSend(conn, err.dump());
        return;
    }
    js["to"] = toid;
    js["toid"] = toid;
    int fromid = js.contains("id") ? js["id"].get<int>() : 0;
    if (!js.contains("time")) {
        time_t now = ::time(nullptr);
        char buf[64] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        js["time"] = buf;
    }

    // 1) 先落库（私聊 groupid=0），status=0；对端 ACK 后标已送达
    string payload = js.dump();
    long long msgId = _msgModel.insert(fromid, toid, payload, 0, 0);
    js["msg_id"] = msgId;
    payload = js.dump();

    // 回给发送方 msg_id，前端可去掉乐观发送的重复气泡
    {
        json ack = js;
        ack["msgid"] = ONE_CHAT_MSG;
        ack["echo"] = true;
        ClientSend(conn, ack.dump());
    }

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            ClientSend(it->second, payload);
            return;
        }
    }

    User peer = _userModel.query(toid);
    if (peer.getState() == "online") {
        _redis.publish(toid, payload);
        return;
    }

    json tip;
    tip["type"] = "system";
    tip["msg"] = "peer offline, saved (msg_id=" + to_string(msgId) + ")";
    tip["msg_id"] = msgId;
    ClientSend(conn, tip.dump());
}

void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        // 仅清理当前连接，避免旧连接迟到的 LOGINOUT 误删新会话
        if (it != _userConnMap.end() && (!conn || it->second == conn)) {
            _userConnMap.erase(it);
        } else if (!conn) {
            _userConnMap.erase(userid);
        } else {
            return;
        }
    }
    BindConnUser(conn, -1);
    _redis.unsubscribe(userid);
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
    notifyPresence(userid, false);
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    int userid = BoundUserId(conn);
    {
        lock_guard<mutex> lock(_connMutex);
        if (userid == -1) {
            for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it) {
                if (it->second == conn) {
                    userid = it->first;
                    _userConnMap.erase(it);
                    break;
                }
            }
        } else {
            auto it = _userConnMap.find(userid);
            if (it != _userConnMap.end() && it->second == conn) {
                _userConnMap.erase(it);
            } else {
                // 连接已不是当前会话（被顶号），不要把新会话打成 offline
                userid = -1;
            }
        }
    }
    if (userid != -1) {
        BindConnUser(conn, -1);
        _redis.unsubscribe(userid);
        User user(userid, "", "", "offline");
        _userModel.updateState(user);
        notifyPresence(userid, false);
        LOG_INFO << "user " << userid << " offline (disconnect) on " << _gatewayId;
    }
}

void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    // 双向好友，两边登录都能看见
    _friendModel.insert(userid, friendid);
    _friendModel.insert(friendid, userid);

    json response;
    response["type"] = "system";
    response["msg"] = "add friend ok";
    response["errno"] = 0;
    vector<string> vec2;
    for (auto& fr : _friendModel.query(userid)) {
        json item;
        UserProfile p;
        if (UserRpcClient::Instance().GetUserProfile(fr.getId(), &p)) {
            item["id"] = p.id;
            item["name"] = p.name;
            item["state"] = p.state;
            item["avatar"] = p.avatar.empty() ? avatarOf(p.id) : p.avatar;
        } else {
            item["id"] = fr.getId();
            item["name"] = fr.getName();
            item["state"] = fr.getState();
            item["avatar"] = avatarOf(fr.getId());
        }
        vec2.push_back(item.dump());
    }
    response["friends"] = vec2;
    response["friend"] = vec2;
    ClientSend(conn, response.dump());
}

void ChatService::delFriend(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.remove(userid, friendid);

    json response;
    response["type"] = "system";
    response["msg"] = "del friend ok";
    response["errno"] = 0;
    vector<string> vec2;
    for (auto& fr : _friendModel.query(userid)) {
        json item;
        UserProfile p;
        if (UserRpcClient::Instance().GetUserProfile(fr.getId(), &p)) {
            item["id"] = p.id;
            item["name"] = p.name;
            item["state"] = p.state;
            item["avatar"] = p.avatar.empty() ? avatarOf(p.id) : p.avatar;
        } else {
            item["id"] = fr.getId();
            item["name"] = fr.getName();
            item["state"] = fr.getState();
            item["avatar"] = avatarOf(fr.getId());
        }
        vec2.push_back(item.dump());
    }
    response["friends"] = vec2;
    response["friend"] = vec2;
    ClientSend(conn, response.dump());
}

void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    Group group(-1, js["groupname"], js["groupdesc"]);
    json response;
    response["type"] = "system";
    response["errno"] = 1;
    response["msg"] = "create group failed";
    if (_groupModel.createGroup(group)) {
        _groupModel.addGroup(userid, group.getId(), "creator");
        response["errno"] = 0;
        response["msg"] = "create group ok";
        response["groupid"] = group.getId();
    }
    response["groups"] = groupsPayload(userid);
    ClientSend(conn, response.dump());
}

void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
    json response;
    response["type"] = "system";
    response["errno"] = 0;
    response["msg"] = "add group ok";
    response["groups"] = groupsPayload(userid);
    ClientSend(conn, response.dump());
}

void ChatService::quitGroup(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.quitGroup(userid, groupid);
    json response;
    response["type"] = "system";
    response["errno"] = 0;
    response["msg"] = "quit group ok";
    response["groups"] = groupsPayload(userid);
    ClientSend(conn, response.dump());
}

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    User sender = _userModel.query(userid);
    js["name"] = sender.getName().empty() ? ("user" + to_string(userid)) : sender.getName();
    js["avatar"] = avatarOf(userid);
    js["msgid"] = GROUP_CHAT_MSG;
    if (!js.contains("time")) {
        time_t now = ::time(nullptr);
        char buf[64] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        js["time"] = buf;
    }

    // 群会话归档：toid=0 + groupid，刷新后可拉取
    string payload = js.dump();
    long long msgId = _msgModel.insert(userid, 0, payload, 1, groupid);
    js["msg_id"] = msgId;
    payload = js.dump();

    // 回显给发送方（带 msg_id，去重乐观气泡）
    {
        json echo = js;
        echo["echo"] = true;
        ClientSend(conn, echo.dump());
    }

    vector<int> ids = _groupModel.queryGroupUsers(userid, groupid);
    for (int id : ids) {
        bool local = false;
        {
            lock_guard<mutex> lock(_connMutex);
            auto it = _userConnMap.find(id);
            if (it != _userConnMap.end()) {
                ClientSend(it->second, payload);
                local = true;
            }
        }
        if (local) continue;
        User u = _userModel.query(id);
        if (u.getState() == "online") _redis.publish(id, payload);
        else _msgModel.insert(userid, id, payload, 0, groupid);
    }
}

void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    // presence 包也走同一频道
    try {
        json j = json::parse(msg);
        if (j.contains("type") && j["type"] == "presence") {
            lock_guard<mutex> lock(_connMutex);
            auto it = _userConnMap.find(userid);
            if (it != _userConnMap.end()) ClientSend(it->second, msg);
            return;
        }
    } catch (...) {
    }

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end()) {
        ClientSend(it->second, msg);
        // 不在这里 markDelivered：等客户端 OFFLINE_MSG_ACK / 收到后的 ACK
        return;
    }
}
