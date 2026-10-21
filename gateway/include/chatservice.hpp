#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include <muduo/net/TcpConnection.h>
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "json.hpp"
#include "messagemodel.hpp"
#include "redis.hpp"
#include "usermodel.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

using MsgHandler = function<void(const TcpConnectionPtr& conn, json& js, Timestamp time)>;

class ChatService
{
public:
    static ChatService* instance();
    void setGatewayId(const string& id) { _gatewayId = id; }
    const string& gatewayId() const { return _gatewayId; }

    void login(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void reg(const TcpConnectionPtr& conn, json& js, Timestamp time);
    MsgHandler getHandler(int msgid);
    void clientCloseException(const TcpConnectionPtr& conn);
    void loginout(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void offlineAck(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void loadHistory(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void loadGroupHistory(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void reset();
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void delFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void quitGroup(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void handleRedisSubscribeMessage(int, string);
    // 仅把本机连接上的用户置 offline（多 Gateway 时不要全表 reset）
    void offlineLocalUsers();

private:
    ChatService();
    string avatarOf(int userid);
    void notifyPresence(int userid, bool online);
    json groupToJson(Group& g);
    vector<string> groupsPayload(int userid);

    unordered_map<int, MsgHandler> _msgHandlerMap;
    UserModel _userModel;
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    mutex _connMutex;
    MessageModel _msgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    Redis _redis;
    string _gatewayId{"gw-1"};
};
#endif
