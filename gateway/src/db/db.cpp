#include "db.hpp"

MySQL::MySQL() { _conn = mysql_init(nullptr); }

bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(), password.c_str(), dbname.c_str(), dbport, nullptr, 0);
    if (p != nullptr)
    {
        mysql_query(_conn, "set names utf8mb4");
        LOG_INFO << "connect to mysql success!";
    }
    else
    {
        LOG_ERROR << "connect to mysql failed!";
    }
    return p;
}
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败!";
        return false;
    }
    return true;
}
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

MYSQL* MySQL::getConnection()
{
    return _conn;
}

MySQL::~MySQL()
{
    if (_conn != nullptr)
        mysql_close(_conn);
}