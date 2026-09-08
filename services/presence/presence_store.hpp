#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

struct PresenceRecord {
    bool online = false;
    std::string gateway_id;
    int64_t last_seen_ms = 0;
};

class PresenceStore {
public:
    bool Register(int userid, const std::string& gateway_id, int64_t now_ms);
    bool Unregister(int userid, const std::string& gateway_id);
    bool Query(int userid, PresenceRecord* out) const;
    bool Kick(int userid, PresenceRecord* previous);
    std::vector<std::pair<int, PresenceRecord>> Snapshot() const;

private:
    mutable std::mutex mu_;
    std::unordered_map<int, PresenceRecord> map_;
};
