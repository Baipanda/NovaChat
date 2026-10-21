#include "rpcconn.h"
#include "rpcframe.h"
#include "rpcheader.pb.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int64_t NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool RecvAll(int fd, char* buf, size_t n)
{
    size_t got = 0;
    while (got < n) {
        ssize_t r = ::recv(fd, buf + got, n - got, 0);
        if (r == 0) return false;
        if (r < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        got += static_cast<size_t>(r);
    }
    return true;
}

bool SendAll(int fd, const char* buf, size_t n)
{
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = ::send(fd, buf + sent, n - sent, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        sent += static_cast<size_t>(w);
    }
    return true;
}

void EnableTcpKeepAlive(int fd)
{
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
    int idle = 30;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    int intvl = 5;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    int cnt = 3;
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
}

}  // namespace

std::mutex RpcLongConnection::pool_mu_;
std::unordered_map<std::string, std::weak_ptr<RpcLongConnection>> RpcLongConnection::pool_;

std::shared_ptr<RpcLongConnection> RpcLongConnection::Get(const std::string& ip, uint16_t port)
{
    const std::string key = ip + ":" + std::to_string(port);
    std::lock_guard<std::mutex> lock(pool_mu_);
    auto it = pool_.find(key);
    if (it != pool_.end()) {
        if (auto sp = it->second.lock()) return sp;
    }
    std::shared_ptr<RpcLongConnection> sp(new RpcLongConnection(ip, port));
    pool_[key] = sp;
    return sp;
}

RpcLongConnection::RpcLongConnection(std::string ip, uint16_t port)
    : ip_(std::move(ip)), port_(port)
{
    last_active_ms_ = NowMs();
}

RpcLongConnection::~RpcLongConnection()
{
    Close();
}

void RpcLongConnection::TouchActive()
{
    last_active_ms_ = NowMs();
    miss_heartbeat_ = 0;
}

void RpcLongConnection::Close()
{
    stop_ = true;
    {
        std::lock_guard<std::mutex> lock(conn_mu_);
        MarkDeadLocked();
    }
    FailAllPending(RpcErr::kConnClosed);
    if (heartbeat_.joinable() && std::this_thread::get_id() != heartbeat_.get_id()) {
        heartbeat_.join();
    } else if (heartbeat_.joinable()) {
        heartbeat_.detach();
    }
    if (reader_.joinable() && std::this_thread::get_id() != reader_.get_id()) {
        reader_.join();
    } else if (reader_.joinable()) {
        reader_.detach();
    }
}

void RpcLongConnection::MarkDeadLocked()
{
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

void RpcLongConnection::FailAllPending(int err)
{
    std::unordered_map<uint32_t, std::shared_ptr<Pending>> copy;
    {
        std::lock_guard<std::mutex> lock(pending_mu_);
        copy.swap(pending_);
    }
    for (auto& p : copy) {
        std::lock_guard<std::mutex> lk(p.second->mu);
        p.second->done = true;
        p.second->err = err;
        p.second->cv.notify_one();
    }
}

bool RpcLongConnection::EnsureConnectedLocked()
{
    if (fd_ >= 0) return true;

    // Join old threads without holding conn_mu_ (avoid deadlock with ReaderLoop).
    std::thread old_reader;
    std::thread old_hb;
    old_reader.swap(reader_);
    old_hb.swap(heartbeat_);
    // Temporarily unlock pattern: caller holds lock; we unlock around join.
    // This method is only called with conn_mu_ locked by Call — release & reacquire.
    // Use a nested unlock via unique_lock in Call instead; here detach stale threads.
    if (old_reader.joinable()) old_reader.detach();
    if (old_hb.joinable()) old_hb.detach();

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    EnableTcpKeepAlive(fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = inet_addr(ip_.c_str());
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[rpc] connect " << ip_ << ":" << port_ << " errno=" << errno << std::endl;
        ::close(fd);
        return false;
    }

    fd_ = fd;
    stop_ = false;
    miss_heartbeat_ = 0;
    TouchActive();

    auto self = shared_from_this();
    reader_ = std::thread([self]() { self->ReaderLoop(); });
    heartbeat_ = std::thread([self]() { self->HeartbeatLoop(); });
    std::cout << "[rpc] long-conn " << ip_ << ":" << port_ << std::endl;
    return true;
}

bool RpcLongConnection::SendRaw(const std::string& frame)
{
    std::lock_guard<std::mutex> lock(send_mu_);
    int fd = -1;
    {
        std::lock_guard<std::mutex> clk(conn_mu_);
        fd = fd_;
    }
    if (fd < 0) return false;
    if (!SendAll(fd, frame.data(), frame.size())) {
        std::lock_guard<std::mutex> clk(conn_mu_);
        MarkDeadLocked();
        return false;
    }
    return true;
}

void RpcLongConnection::HeartbeatLoop()
{
    while (!stop_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kHeartbeatCheckMs));
        if (stop_) break;

        int fd = -1;
        {
            std::lock_guard<std::mutex> lock(conn_mu_);
            fd = fd_;
        }
        if (fd < 0) continue;

        const int64_t idle = NowMs() - last_active_ms_.load();
        if (idle < kIdlePingMs) continue;

        if (miss_heartbeat_.load() >= kMaxMissHeartbeat) {
            std::cerr << "[rpc] heartbeat fail, close " << ip_ << ":" << port_ << std::endl;
            {
                std::lock_guard<std::mutex> lock(conn_mu_);
                MarkDeadLocked();
            }
            FailAllPending(RpcErr::kHeartbeatFail);
            continue;
        }

        mprpc::RpcHeader h;
        h.set_msg_type(RpcMsg::kPing);
        h.set_request_id(0);
        h.set_args_size(0);
        h.set_errcode(0);
        if (!SendRaw(RpcFrame::Encode(h, ""))) {
            FailAllPending(RpcErr::kSendFail);
            continue;
        }
        miss_heartbeat_.fetch_add(1);
    }
}

void RpcLongConnection::ReaderLoop()
{
    std::string buf;
    buf.reserve(4096);
    char tmp[4096];

    while (!stop_) {
        int fd = -1;
        {
            std::lock_guard<std::mutex> lock(conn_mu_);
            fd = fd_;
        }
        if (fd < 0) break;

        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        buf.append(tmp, static_cast<size_t>(n));
        TouchActive();

        for (;;) {
            mprpc::RpcHeader header;
            std::string body;
            size_t consumed = 0;
            int derr = 0;
            bool ok = RpcFrame::TryDecode(buf.data(), buf.size(), &consumed, &header, &body, &derr);
            if (!ok) {
                if (derr != 0) {
                    // bad magic/version/size — tear down
                    buf.clear();
                    goto reader_exit;
                }
                break;  // need more data
            }
            buf.erase(0, consumed);

            if (header.msg_type() == RpcMsg::kPing) {
                mprpc::RpcHeader ack;
                ack.set_msg_type(RpcMsg::kAck);
                ack.set_request_id(header.request_id());
                ack.set_args_size(0);
                ack.set_errcode(0);
                SendRaw(RpcFrame::Encode(ack, ""));
                continue;
            }
            if (header.msg_type() == RpcMsg::kAck) {
                TouchActive();
                continue;
            }
            if (header.msg_type() != RpcMsg::kResponse) {
                continue;
            }

            std::shared_ptr<Pending> pend;
            {
                std::lock_guard<std::mutex> lock(pending_mu_);
                auto it = pending_.find(header.request_id());
                if (it != pending_.end()) {
                    pend = it->second;
                    pending_.erase(it);
                }
            }
            if (!pend) continue;  // late / unknown

            {
                std::lock_guard<std::mutex> lk(pend->mu);
                pend->header_errcode = header.errcode();
                pend->body = std::move(body);
                pend->err = (header.errcode() == 0) ? RpcErr::kOk : header.errcode();
                pend->done = true;
            }
            pend->cv.notify_one();
        }
    }

reader_exit:
    {
        std::lock_guard<std::mutex> lock(conn_mu_);
        MarkDeadLocked();
    }
    FailAllPending(RpcErr::kConnClosed);
}

int RpcLongConnection::Call(const std::string& frame, uint32_t request_id,
                            std::string* response_body, int* header_errcode, int timeout_ms)
{
    {
        std::lock_guard<std::mutex> lock(conn_mu_);
        if (!EnsureConnectedLocked()) return RpcErr::kConnectFail;
    }

    auto pend = std::make_shared<Pending>();
    {
        std::lock_guard<std::mutex> lock(pending_mu_);
        pending_[request_id] = pend;
    }

    if (!SendRaw(frame)) {
        std::lock_guard<std::mutex> lock(pending_mu_);
        pending_.erase(request_id);
        return RpcErr::kSendFail;
    }
    TouchActive();

    std::unique_lock<std::mutex> lk(pend->mu);
    bool got = pend->cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                                 [&] { return pend->done; });
    if (!got) {
        lk.unlock();
        std::lock_guard<std::mutex> lock(pending_mu_);
        pending_.erase(request_id);
        return RpcErr::kTimeout;
    }
    if (header_errcode) *header_errcode = pend->header_errcode;
    if (pend->err != RpcErr::kOk) return pend->err;
    if (response_body) *response_body = pend->body;
    return RpcErr::kOk;
}
