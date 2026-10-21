#pragma once

#include "rpcerror.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// TCP long connection: request_id demux + idle PING/ACK + TCP keepalive.
class RpcLongConnection : public std::enable_shared_from_this<RpcLongConnection>
{
public:
    static std::shared_ptr<RpcLongConnection> Get(const std::string& ip, uint16_t port);

    // Encode+send request frame; wait for matching RESPONSE (or transport errcode in header).
    // Returns RpcErr::* ; on success fills response_body.
    int Call(const std::string& frame, uint32_t request_id,
             std::string* response_body, int* header_errcode, int timeout_ms);

    void Close();
    ~RpcLongConnection();

private:
    RpcLongConnection(std::string ip, uint16_t port);

    bool EnsureConnectedLocked();
    void ReaderLoop();
    void HeartbeatLoop();
    void FailAllPending(int err);
    void MarkDeadLocked();
    bool SendRaw(const std::string& frame);
    void TouchActive();

    struct Pending
    {
        std::mutex mu;
        std::condition_variable cv;
        bool done = false;
        int err = RpcErr::kOk;
        int header_errcode = 0;
        std::string body;
    };

    std::string ip_;
    uint16_t port_;
    int fd_ = -1;
    std::mutex conn_mu_;
    std::mutex send_mu_;
    std::mutex pending_mu_;
    std::unordered_map<uint32_t, std::shared_ptr<Pending>> pending_;
    std::thread reader_;
    std::thread heartbeat_;
    std::atomic<bool> stop_{false};
    std::atomic<int64_t> last_active_ms_{0};
    std::atomic<int> miss_heartbeat_{0};

    enum { kIdlePingMs = 10000, kHeartbeatCheckMs = 2000, kMaxMissHeartbeat = 3 };

    static std::mutex pool_mu_;
    static std::unordered_map<std::string, std::weak_ptr<RpcLongConnection>> pool_;
};
