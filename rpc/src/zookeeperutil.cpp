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
    char path_buffer[128] = {0};
    int bufferlen = sizeof(path_buffer);
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZNONODE) {
        flag = zoo_create(m_zhandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, flags, path_buffer, bufferlen);
        if (flag == ZOK) {
            std::cout << "[zk] create success: " << path << std::endl;
        } else {
            std::cout << "[zk] create failed: " << path << " flag=" << flag << std::endl;
        }
    }
}

std::string ZkClient::GetData(const char* path)
{
    char buffer[128] = {0};
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK) {
        std::cout << "[zk] get data failed: " << path << std::endl;
        return "";
    }
    return buffer;
}

std::vector<std::string> ZkClient::GetChildren(const char* path)
{
    std::vector<std::string> out;
    struct String_vector children;
    memset(&children, 0, sizeof(children));
    int flag = zoo_get_children(m_zhandle, path, 0, &children);
    if (flag != ZOK) {
        std::cout << "[zk] get children failed: " << path << " flag=" << flag << std::endl;
        return out;
    }
    for (int i = 0; i < children.count; ++i) {
        out.emplace_back(children.data[i]);
    }
    deallocate_String_vector(&children);
    return out;
}
