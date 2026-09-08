#ifndef DB_H
#define DB_H

#include <muduo/base/Logging.h>
#include <mysql/mysql.h>
#include <string>
using namespace std;

static string server = "127.0.0.1";
static string user = "root";
static string password = "100103";
static string dbname = "chat";
// Docker demo maps container 3306 -> host 3307; local mysqld 仍可用 3306
#ifndef NOVA_MYSQL_PORT
#define NOVA_MYSQL_PORT 3307
#endif
static unsigned int dbport = NOVA_MYSQL_PORT;
class MySQL
{
public:
    // 初始化数据库
    MySQL();
    // 连接数据库
    bool connect();
    // 更新操作
    bool update(string sql);
    // 查询操作
    MYSQL_RES *query(string sql);
    // 获取数据库连接
    MYSQL* getConnection();
    // 释放数据库连接资源
    ~MySQL();
private:
    MYSQL *_conn;
};
#endif