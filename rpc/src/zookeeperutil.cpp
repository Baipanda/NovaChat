#include "zookeeperutil.h"
#include "mprpcapplication.h"
#include <iostream>
#include <cstring>

void global_watcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    (void)path;
    (void)watcherCtx;
    if (type == ZOO_SESSION_EVENT) {
        if (state == ZOO_CONNECTED_STATE) {
            sem_t* sem = (sem_t*)zoo_get_context(zh);
            if (sem) sem_post(sem);
        }
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr) {}

ZkClient::~ZkClient()
{
    if (m_zhandle) {
        zookeeper_close(m_zhandle);
        m_zhandle = nullptr;
    }
}

void ZkClient::Start()
{
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    if (host.empty()) host = "127.0.0.1";
    if (port.empty()) port = "2181";
    std::string connstr = host + ":" + port;

    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if (!m_zhandle) {
        std::cout << "[zk] zookeeper_init error" << std::endl;
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);
    sem_wait(&sem);
    sem_destroy(&sem);
    std::cout << "[zk] connected to " << connstr << std::endl;
}

void ZkClient::Create(const char* path, const char* data, int datalen, int flags)
{
    if (!m_zhandle || !path) return;

    char path_buffer[256] = {0};
    int bufferlen = sizeof(path_buffer);

    int rc = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, flags,
                        path_buffer, bufferlen);
    if (rc == ZOK) {
        std::cout << "[zk] create success: " << path
                  << (flags & ZOO_EPHEMERAL ? " (ephemeral)" : "") << std::endl;
        return;
    }

    if (rc == ZNODEEXISTS) {
        // 持久节点已存在：正常；临时节点若仍在（同会话重复注册）刷新 data
        if (data && datalen > 0) {
            int set_rc = zoo_set(m_zhandle, path, data, datalen, -1);
            if (set_rc != ZOK) {
                std::cout << "[zk] set data on existing node: " << path
                          << " rc=" << set_rc << " " << zerror(set_rc) << std::endl;
            }
        }
        std::cout << "[zk] already exists: " << path << std::endl;
        return;
    }

    // 临时节点若被旧会话占着（少见），删了再建
    if ((flags & ZOO_EPHEMERAL) && rc != ZOK) {
        zoo_delete(m_zhandle, path, -1);
        rc = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, flags,
                        path_buffer, bufferlen);
        if (rc == ZOK) {
            std::cout << "[zk] recreate success: " << path << " (ephemeral)" << std::endl;
            return;
        }
    }

    std::cout << "[zk] create failed: " << path << " rc=" << rc << " " << zerror(rc)
              << std::endl;
}

std::string ZkClient::GetData(const char* path)
{
    char buffer[256] = {0};
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK) {
        std::cout << "[zk] get data failed: " << path << " rc=" << flag << " "
                  << zerror(flag) << std::endl;
        return "";
    }
    if (bufferlen < 0) return "";
    return std::string(buffer, static_cast<size_t>(bufferlen));
}

std::vector<std::string> ZkClient::GetChildren(const char* path)
{
    std::vector<std::string> out;
    struct String_vector children;
    memset(&children, 0, sizeof(children));
    int flag = zoo_get_children(m_zhandle, path, 0, &children);
    if (flag != ZOK) {
        std::cout << "[zk] get children failed: " << path << " rc=" << flag << " "
                  << zerror(flag) << std::endl;
        return out;
    }
    for (int i = 0; i < children.count; ++i) {
        out.emplace_back(children.data[i]);
    }
    deallocate_String_vector(&children);
    return out;
}
