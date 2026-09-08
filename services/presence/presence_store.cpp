#include "presence_store.hpp"

bool PresenceStore::Register(int userid, const std::string& gateway_id, int64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mu_);
    PresenceRecord& rec = map_[userid];
    rec.online = true;
    rec.gateway_id = gateway_id;
    rec.last_seen_ms = now_ms;
    return true;
}

bool PresenceStore::Unregister(int userid, const std::string& gateway_id)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = map_.find(userid);
    if (it == map_.end()) return true;
    if (it->second.gateway_id == gateway_id) {
        map_.erase(it);
    }
    return true;
}

bool PresenceStore::Query(int userid, PresenceRecord* out) const
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = map_.find(userid);
    if (it == map_.end()) {
        if (out) {
            out->online = false;
            out->gateway_id.clear();
            out->last_seen_ms = 0;
        }
        return false;
    }
    if (out) *out = it->second;
    return true;
}

bool PresenceStore::Kick(int userid, PresenceRecord* previous)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = map_.find(userid);
    if (it == map_.end()) return false;
    if (previous) *previous = it->second;
    map_.erase(it);
    return true;
}

std::vector<std::pair<int, PresenceRecord>> PresenceStore::Snapshot() const
{
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::pair<int, PresenceRecord>> out;
    out.reserve(map_.size());
    for (const auto& kv : map_) out.push_back(kv);
    return out;
}
