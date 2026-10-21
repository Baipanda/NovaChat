#pragma once

#include "rpcheader.pb.h"

#include <arpa/inet.h>
#include <cstdint>
#include <cstring>
#include <string>

// Wire frame:
//   magic(4)="NRPC" | version(1)=1 | header_len(4, network order) | RpcHeader | body
namespace RpcFrame {

constexpr char kMagic[4] = {'N', 'R', 'P', 'C'};
constexpr uint8_t kVersion = 1;
constexpr size_t kPrefixLen = 9;  // 4 + 1 + 4
constexpr uint32_t kMaxHeaderLen = 4 * 1024;
constexpr uint32_t kMaxBodyLen = 1 * 1024 * 1024;

inline bool MatchMagic(const char* p)
{
    return p[0] == kMagic[0] && p[1] == kMagic[1] && p[2] == kMagic[2] && p[3] == kMagic[3];
}

inline std::string Encode(const mprpc::RpcHeader& header, const std::string& body)
{
    std::string header_str;
    header.SerializeToString(&header_str);
    uint32_t header_len_ne = htonl(static_cast<uint32_t>(header_str.size()));

    std::string out;
    out.reserve(kPrefixLen + header_str.size() + body.size());
    out.append(kMagic, 4);
    out.push_back(static_cast<char>(kVersion));
    out.append(reinterpret_cast<const char*>(&header_len_ne), 4);
    out += header_str;
    out += body;
    return out;
}

// Peek one complete frame from buffer. Returns true if a full frame is available.
// On success, advances *consumed and fills header/body.
inline bool TryDecode(const char* data, size_t len, size_t* consumed,
                      mprpc::RpcHeader* header, std::string* body, int* err)
{
    *consumed = 0;
    if (err) *err = 0;
    if (len < kPrefixLen) return false;

    if (!MatchMagic(data)) {
        if (err) *err = 1004;  // invalid_request / bad magic
        return false;
    }
    uint8_t ver = static_cast<uint8_t>(data[4]);
    if (ver != kVersion) {
        if (err) *err = 1004;
        return false;
    }

    uint32_t header_len_ne = 0;
    std::memcpy(&header_len_ne, data + 5, 4);
    uint32_t header_len = ntohl(header_len_ne);
    if (header_len == 0 || header_len > kMaxHeaderLen) {
        if (err) *err = 1004;
        return false;
    }
    if (len < kPrefixLen + header_len) return false;

    if (!header->ParseFromArray(data + kPrefixLen, static_cast<int>(header_len))) {
        if (err) *err = 1004;
        return false;
    }
    if (header->args_size() > kMaxBodyLen) {
        if (err) *err = 1004;
        return false;
    }
    size_t total = kPrefixLen + header_len + header->args_size();
    if (len < total) return false;

    if (body) {
        body->assign(data + kPrefixLen + header_len, header->args_size());
    }
    *consumed = total;
    return true;
}

}  // namespace RpcFrame
