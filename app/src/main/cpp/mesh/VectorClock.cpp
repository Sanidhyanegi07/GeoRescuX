#include "VectorClock.h"
#include <algorithm>
#include <cstring>

namespace georescux::mesh {

VectorClock::VectorClock(std::string localNodeId)
    : localNodeId_(std::move(localNodeId)) {
    if (!localNodeId_.empty()) {
        clocks_[localNodeId_] = 0;
    }
}

void VectorClock::tick() noexcept {
    if (!localNodeId_.empty()) {
        clocks_[localNodeId_]++;
    }
}

void VectorClock::merge(const VectorClock& other) {
    for (const auto& [nodeId, time] : other.clocks_) {
        clocks_[nodeId] = std::max(clocks_[nodeId], time);
    }
    tick();
}

CausalOrdering VectorClock::compare(const VectorClock& a, const VectorClock& b) noexcept {
    bool hasGreater = false;
    bool hasSmaller = false;

    // Check all keys in a
    for (const auto& [nodeId, timeA] : a.clocks_) {
        uint64_t timeB = b.getClock(nodeId);
        if (timeA > timeB) hasGreater = true;
        if (timeA < timeB) hasSmaller = true;
    }

    // Check remaining keys in b
    for (const auto& [nodeId, timeB] : b.clocks_) {
        if (!a.clocks_.count(nodeId)) {
            if (timeB > 0) hasSmaller = true;
        }
    }

    if (!hasGreater && !hasSmaller) return CausalOrdering::IDENTICAL;
    if (hasGreater && !hasSmaller) return CausalOrdering::HAPPENED_AFTER;
    if (!hasGreater && hasSmaller) return CausalOrdering::HAPPENED_BEFORE;
    return CausalOrdering::CONCURRENT;
}

void VectorClock::setClock(const std::string& nodeId, uint64_t val) {
    clocks_[nodeId] = val;
}

uint64_t VectorClock::getClock(const std::string& nodeId) const {
    auto it = clocks_.find(nodeId);
    return it != clocks_.end() ? it->second : 0ULL;
}

std::vector<uint8_t> VectorClock::serialize() const {
    std::vector<uint8_t> buf;
    const uint16_t entriesCount = static_cast<uint16_t>(clocks_.size());
    buf.resize(2);
    buf[0] = static_cast<uint8_t>(entriesCount >> 8);
    buf[1] = static_cast<uint8_t>(entriesCount & 0xFF);

    for (const auto& [nodeId, time] : clocks_) {
        const uint8_t idLen = static_cast<uint8_t>(std::min<size_t>(255, nodeId.size()));
        buf.push_back(idLen);
        buf.insert(buf.end(), nodeId.data(), nodeId.data() + idLen);

        // 8 bytes for time (big endian)
        for (int i = 7; i >= 0; --i) {
            buf.push_back(static_cast<uint8_t>(time >> (i * 8)));
        }
    }
    return buf;
}

VectorClock VectorClock::deserialize(const uint8_t* data, size_t length) {
    VectorClock clock;
    if (!data || length < 2) return clock;

    const uint16_t entries = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    size_t offset = 2;

    for (uint16_t i = 0; i < entries && offset < length; ++i) {
        uint8_t idLen = data[offset++];
        if (offset + idLen + 8 > length) break;

        std::string nodeId(reinterpret_cast<const char*>(data + offset), idLen);
        offset += idLen;

        uint64_t time = 0;
        for (int b = 0; b < 8; ++b) {
            time = (time << 8) | data[offset++];
        }

        clock.setClock(nodeId, time);
    }

    return clock;
}

} // namespace georescux::mesh
