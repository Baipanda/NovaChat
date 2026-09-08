#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <unordered_map>
#include <functional>
#include <mutex>
#include <string>
#include <muduo/net/TcpConnection.h>
#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

using MsgHandler = function<void (const TcpConnectionPtr& conn, json& js, Timestamp time)>;
// 聊天服务器业务类
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
    void reset();
    void addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time);
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    void handleRedisSubscribeMessage(int, string);
    void broadcastPresence();

private:
    ChatService();
    string presenceStateOf(int userid);

    unordered_map<int, MsgHandler> _msgHandlerMap;
    UserModel _userModel;
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    mutex _connMutex;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    Redis _redis;
    string _gatewayId{"gw-1"};
};
#endif
