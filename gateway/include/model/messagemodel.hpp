#ifndef MESSAGEMODEL_H
#define MESSAGEMODEL_H

#include <string>
#include <vector>
using namespace std;

struct ChatMessage {
    long long id = 0;
    int fromid = 0;
    int toid = 0;
    int groupid = 0;  // 0=私聊；>0=群聊
    string message;   // JSON payload
    int status = 0;   // 0=pending/unread  1=delivered/acked / 群历史归档
    string createtime;
};

// Persist-first message store
class MessageModel
{
public:
    void ensureTable();
    // groupid=0 私聊；群历史归档用 toid=0 + groupid>0；群离线投递用 toid=成员
    long long insert(int fromid, int toid, const string& message, int status, int groupid = 0);
    void markDelivered(const vector<long long>& ids);
    vector<ChatMessage> queryPending(int toid);
    // 仅私聊（groupid=0）
    vector<ChatMessage> queryHistory(int uidA, int uidB, int limit = 50);
    // 群会话历史（canonical：groupid>0 且 toid=0）
    vector<ChatMessage> queryGroupHistory(int groupid, int limit = 50);
};

#endif
