#include "chatservice.hpp"
#include "clientsend.hpp"
#include "presence_rpc_client.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include <map>
#include <ctime>

using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
// 实现类内静态函数的时候就不用写static了
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的回调操作
ChatService::ChatService()
{
    // 任何需要传入函数的位置，如果因为函数是类内的，导致自动加入了this，都可以使用bind来解决这个问题
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addFriend, this, _1, _2, _3)});
    
    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if(_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

void ChatService::reset()
{
    // 将online状态的用户，设置为offline
    _userModel.resetState();
}

string ChatService::presenceStateOf(int userid)
{
    PresenceView v;
    if (PresenceRpcClient::Instance().Query(userid, &v)) {
        return v.online ? "online" : "offline";
    }
    // RPC 失败时回退 MySQL
    User u = _userModel.query(userid);
    return u.getState().empty() ? string("offline") : u.getState();
}

void ChatService::broadcastPresence()
{
    json js;
    js["type"] = "presence";
    json arr = json::array();
    for (const auto& v : PresenceRpcClient::Instance().List()) {
        json item;
        item["userid"] = v.userid;
        item["online"] = v.online;
        item["gateway_id"] = v.gateway_id;
        item["last_seen_ms"] = v.last_seen_ms;
        arr.push_back(item);
    }
    js["list"] = arr;
    string payload = js.dump();

    lock_guard<mutex> lock(_connMutex);
    for (auto& p : _userConnMap) {
        ClientSend(p.second, payload);
    }
}

// 实现登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录";
            ClientSend(conn, response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                // 临界区代码段
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            
            // id用户登录成功，向redis订阅channel(id)
            _redis.subscribe(id);
            
            // 更新用户状态信息（MySQL 粗状态仍保留）
            user.setState("online");
            _userModel.updateState(user);

            // Presence RPC：上报本 Gateway 在线路由
            if (!PresenceRpcClient::Instance().Register(id, _gatewayId)) {
                LOG_WARN << "Presence.Register failed uid=" << id;
            }
            
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            response["gateway_id"] = _gatewayId;
            // 查询用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())
            {
                // vector容器可以直接反序列化
                response["offlinemsg"] = vec;
                // 读取该用户离线消息之后，将该用户所有离线消息删除
                _offlineMsgModel.remove(id);
            }
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(auto& fr : userVec)
                {
                    json fjs;
                    fjs["id"] = fr.getId();
                    fjs["name"] = fr.getName();
                    // 在线态以 Presence RPC 为准
                    fjs["state"] = presenceStateOf(fr.getId());
                    vec2.push_back(fjs.dump());
                }
                // 兼容：原客户端有的写 friends、有的写 friend
                response["friends"] = vec2;
                response["friend"] = vec2;
            }

            // 查询用户的群组消息
            // 返回用户加入了哪些群组
            vector<Group> groupserVec = _groupModel.queryGroups(id);
            if(!groupserVec.empty())
            {
                vector<string> groupV;
                for(Group& group : groupserVec)
                {
                json grpjson;
                grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            ClientSend(conn, response.dump());
            // 通知其他在线用户刷新 Presence
            broadcastPresence();
        }
    }
    else
    {
        // 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        ClientSend(conn, response.dump());
    }
}

// 实现注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        ClientSend(conn, response.dump());
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        // dump()：将 JSON 对象序列化为字符串。
        ClientSend(conn, response.dump());
    }
}

// 获取消息对应的处理
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid非法
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid " << msgid << "can't find handler";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 兼容客户端字段：web 用 to，原版终端常用 toid
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
    if (!js.contains("time")) {
        // 简易时间，避免对端显示缺字段
        time_t now = ::time(nullptr);
        char buf[64] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        js["time"] = buf;
    }

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息，服务器主动发送消息给toid用户
            ClientSend(it->second, js.dump());
            return;
        }
    }
    // 当前主机没有找到聊天用户，但是用户显示在线，说明在另一台服务器登录
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        // 借助redis进行消息转发    
        _redis.publish(toid, js.dump());
        return;
    }
    
    // toid 不在线
    _offlineMsgModel.insert(toid, js.dump());
    json tip;
    tip["type"] = "system";
    tip["msg"] = "peer offline, saved to offline box";
    ClientSend(conn, tip.dump());
}

// 处理客户端注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
        int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid); 

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);

    PresenceRpcClient::Instance().Unregister(userid, _gatewayId);
    broadcastPresence();
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从map表中删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    
    // 取消redis订阅
    _redis.unsubscribe(user.getId());

    // 更新用户的"状态信息
    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
        PresenceRpcClient::Instance().Unregister(user.getId(), _gatewayId);
        broadcastPresence();
    }
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);

    json response;
    response["type"] = "system";
    response["msg"] = "add friend ok";
    response["errno"] = 0;
    vector<string> vec2;
    for (auto& user : _friendModel.query(userid))
    {
        json u;
        u["id"] = user.getId();
        u["name"] = user.getName();
        u["state"] = user.getState();
        vec2.push_back(u.dump());
    }
    response["friends"] = vec2;
    response["friend"] = vec2;
    ClientSend(conn, response.dump());
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    json response;
    response["type"] = "system";
    response["errno"] = 1;
    response["msg"] = "create group failed";

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
        response["errno"] = 0;
        response["msg"] = "create group ok";
        response["groupid"] = group.getId();
    }

    vector<string> groupV;
    for (Group& g : _groupModel.queryGroups(userid))
    {
        json grpjson;
        grpjson["id"] = g.getId();
        grpjson["groupname"] = g.getName();
        grpjson["groupdesc"] = g.getDesc();
        vector<string> userV;
        for (GroupUser &user : g.getUsers())
        {
            json u;
            u["id"] = user.getId();
            u["name"] = user.getName();
            u["state"] = user.getState();
            u["role"] = user.getRole();
            userV.push_back(u.dump());
        }
        grpjson["users"] = userV;
        groupV.push_back(grpjson.dump());
    }
    response["groups"] = groupV;
    ClientSend(conn, response.dump());
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");

    json response;
    response["type"] = "system";
    response["errno"] = 0;
    response["msg"] = "add group ok";
    vector<string> groupV;
    for (Group& g : _groupModel.queryGroups(userid))
    {
        json grpjson;
        grpjson["id"] = g.getId();
        grpjson["groupname"] = g.getName();
        grpjson["groupdesc"] = g.getDesc();
        vector<string> userV;
        for (GroupUser &user : g.getUsers())
        {
            json u;
            u["id"] = user.getId();
            u["name"] = user.getName();
            u["state"] = user.getState();
            u["role"] = user.getRole();
            userV.push_back(u.dump());
        }
        grpjson["users"] = userV;
        groupV.push_back(grpjson.dump());
    }
    response["groups"] = groupV;
    ClientSend(conn, response.dump());
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            ClientSend(it->second, js.dump());
        }
        else
        {
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    // 考虑到接收消息的用户可能在消息传递过程中下线
    // 需要做判断，如果下线了，就存储离线消息
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        ClientSend(it->second, msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}