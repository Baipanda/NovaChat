#ifndef PUBLIC_H
#define PUBLIC_H
/*
server 和 client的公共文件
*/
enum EnMsgType
{
    // 登录消息
    LOGIN_MSG = 1,
    // 登录响应消息
    LOGIN_MSG_ACK,
    // 注销信息
    LOGINOUT_MSG,
    // 注册消息
    REG_MSG,
    // 注册相应消息
    REG_MSG_ACK,
    // 聊天消息
    ONE_CHAT_MSG,
    // 添加好友
    ADD_FRIEND_MSG,
    // 创建群组
    CREATE_GROUP_MSG, 
    // 加入群组
    ADD_GROUP_MSG,
    // 群聊天
    GROUP_CHAT_MSG,
    // 离线/未读消息送达 ACK（客户端确认后服务端更新 message.status）
    OFFLINE_MSG_ACK,
    // 拉取两人会话历史
    LOAD_HISTORY_MSG,
    LOAD_HISTORY_MSG_ACK,
    // 删除好友
    DEL_FRIEND_MSG,
    // 退出群组
    QUIT_GROUP_MSG,
    // 拉取群聊历史
    LOAD_GROUP_HISTORY_MSG,
    LOAD_GROUP_HISTORY_MSG_ACK,
};

#endif