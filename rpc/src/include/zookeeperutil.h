#pragma once

#include <string>
#include <vector>
#include <semaphore.h>
#include <zookeeper/zookeeper.h>

// ZooKeeper 客户端封装：RPC 服务注册 / 发现
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();

    void Start();
    void Create(const char* path, const char* data, int datalen, int flags = 0);
    std::string GetData(const char* path);
    std::vector<std::string> GetChildren(const char* path);

private:
    zhandle_t* m_zhandle;
};
