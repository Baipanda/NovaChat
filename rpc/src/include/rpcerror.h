#pragma once

#include <string>

// Transport / call-level errors. Business codes stay in response body.
namespace RpcErr {
constexpr int kOk = 0;
constexpr int kTimeout = 1001;          // client-local: no response in time
constexpr int kServiceNotFound = 1002;
constexpr int kMethodNotFound = 1003;
constexpr int kInvalidRequest = 1004;
constexpr int kConnectFail = 1005;
constexpr int kSendFail = 1006;
constexpr int kConnClosed = 1007;
constexpr int kHeartbeatFail = 1008;
constexpr int kSerializeFail = 1009;
constexpr int kParseFail = 1010;

inline const char* ToString(int code)
{
    switch (code) {
    case kOk: return "ok";
    case kTimeout: return "timeout";
    case kServiceNotFound: return "service_not_found";
    case kMethodNotFound: return "method_not_found";
    case kInvalidRequest: return "invalid_request";
    case kConnectFail: return "connect_fail";
    case kSendFail: return "send_fail";
    case kConnClosed: return "conn_closed";
    case kHeartbeatFail: return "heartbeat_fail";
    case kSerializeFail: return "serialize_fail";
    case kParseFail: return "parse_fail";
    default: return "unknown";
    }
}
}  // namespace RpcErr

namespace RpcMsg {
constexpr uint32_t kRequest = 1;
constexpr uint32_t kResponse = 2;
constexpr uint32_t kPing = 3;
constexpr uint32_t kAck = 4;
}  // namespace RpcMsg
