#include "messagemodel.hpp"
#include "db.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>

void MessageModel::ensureTable()
{
    MySQL mysql;
    if (!mysql.connect()) return;
    const char* sql =
        "CREATE TABLE IF NOT EXISTS message ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "fromid INT NOT NULL,"
        "toid INT NOT NULL,"
        "groupid INT NOT NULL DEFAULT 0,"
        "message TEXT NOT NULL,"
        "status TINYINT NOT NULL DEFAULT 0,"
        "createtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "KEY idx_peer (fromid, toid, groupid),"
        "KEY idx_group (groupid, toid, id),"
        "KEY idx_pending (toid, status)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    mysql.update(string(sql));
    // 兼容旧表：补 groupid 列（已存在则忽略错误）
    mysql.update("ALTER TABLE message ADD COLUMN groupid INT NOT NULL DEFAULT 0");
    // 把旧的群离线副本从私聊历史里拆出去（payload 里带 groupid）
    mysql.update(
        "UPDATE message SET groupid = CAST(JSON_UNQUOTE(JSON_EXTRACT(message,'$.groupid')) AS UNSIGNED) "
        "WHERE groupid=0 AND JSON_EXTRACT(message,'$.groupid') IS NOT NULL "
        "AND CAST(JSON_UNQUOTE(JSON_EXTRACT(message,'$.groupid')) AS UNSIGNED) > 0");
}

long long MessageModel::insert(int fromid, int toid, const string& message, int status, int groupid)
{
    string esc;
    esc.reserve(message.size());
    for (char c : message) {
        if (c == '\'') esc += "''";
        else esc.push_back(c);
    }
    char sql[8192] = {0};
    snprintf(sql, sizeof(sql),
             "insert into message(fromid,toid,groupid,message,status) values(%d,%d,%d,'%s',%d)",
             fromid, toid, groupid, esc.c_str(), status);
    MySQL mysql;
    if (!mysql.connect()) return 0;
    if (!mysql.update(string(sql))) return 0;

    MYSQL_RES* res = mysql.query(string("select last_insert_id()"));
    if (!res) return 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    long long id = 0;
    if (row && row[0]) id = atoll(row[0]);
    mysql_free_result(res);
    return id;
}

void MessageModel::markDelivered(const vector<long long>& ids)
{
    if (ids.empty()) return;
    ostringstream oss;
    oss << "update message set status=1 where id in (";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) oss << ",";
        oss << ids[i];
    }
    oss << ")";
    MySQL mysql;
    if (mysql.connect()) mysql.update(oss.str().c_str());
}

vector<ChatMessage> MessageModel::queryPending(int toid)
{
    vector<ChatMessage> out;
    char sql[256] = {0};
    snprintf(sql, sizeof(sql),
             "select id,fromid,toid,groupid,message,status,createtime from message "
             "where toid=%d and status=0 order by id asc",
             toid);
    MySQL mysql;
    if (!mysql.connect()) return out;
    MYSQL_RES* res = mysql.query(string(sql));
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        ChatMessage m;
        m.id = row[0] ? atoll(row[0]) : 0;
        m.fromid = row[1] ? atoi(row[1]) : 0;
        m.toid = row[2] ? atoi(row[2]) : 0;
        m.groupid = row[3] ? atoi(row[3]) : 0;
        m.message = row[4] ? row[4] : "";
        m.status = row[5] ? atoi(row[5]) : 0;
        m.createtime = row[6] ? row[6] : "";
        out.push_back(m);
    }
    mysql_free_result(res);
    return out;
}

vector<ChatMessage> MessageModel::queryHistory(int uidA, int uidB, int limit)
{
    vector<ChatMessage> out;
    if (limit <= 0) limit = 50;
    char sql[512] = {0};
    // 只取私聊，避免群离线副本混进二人会话
    snprintf(sql, sizeof(sql),
             "select id,fromid,toid,groupid,message,status,createtime from message "
             "where groupid=0 and ((fromid=%d and toid=%d) or (fromid=%d and toid=%d)) "
             "order by id desc limit %d",
             uidA, uidB, uidB, uidA, limit);
    MySQL mysql;
    if (!mysql.connect()) return out;
    MYSQL_RES* res = mysql.query(string(sql));
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        ChatMessage m;
        m.id = row[0] ? atoll(row[0]) : 0;
        m.fromid = row[1] ? atoi(row[1]) : 0;
        m.toid = row[2] ? atoi(row[2]) : 0;
        m.groupid = row[3] ? atoi(row[3]) : 0;
        m.message = row[4] ? row[4] : "";
        m.status = row[5] ? atoi(row[5]) : 0;
        m.createtime = row[6] ? row[6] : "";
        out.push_back(m);
    }
    mysql_free_result(res);
    std::reverse(out.begin(), out.end());
    return out;
}

vector<ChatMessage> MessageModel::queryGroupHistory(int groupid, int limit)
{
    vector<ChatMessage> out;
    if (groupid <= 0) return out;
    if (limit <= 0) limit = 50;
    char sql[512] = {0};
    // toid=0 为群会话归档行；不含按成员拆分的离线投递副本
    snprintf(sql, sizeof(sql),
             "select id,fromid,toid,groupid,message,status,createtime from message "
             "where groupid=%d and toid=0 order by id desc limit %d",
             groupid, limit);
    MySQL mysql;
    if (!mysql.connect()) return out;
    MYSQL_RES* res = mysql.query(string(sql));
    if (!res) return out;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        ChatMessage m;
        m.id = row[0] ? atoll(row[0]) : 0;
        m.fromid = row[1] ? atoi(row[1]) : 0;
        m.toid = row[2] ? atoi(row[2]) : 0;
        m.groupid = row[3] ? atoi(row[3]) : 0;
        m.message = row[4] ? row[4] : "";
        m.status = row[5] ? atoi(row[5]) : 0;
        m.createtime = row[6] ? row[6] : "";
        out.push_back(m);
    }
    mysql_free_result(res);
    std::reverse(out.begin(), out.end());
    return out;
}
