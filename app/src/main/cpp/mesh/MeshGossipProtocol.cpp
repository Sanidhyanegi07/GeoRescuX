#include "MeshGossipProtocol.h"
#include <algorithm>

namespace georescux::mesh {

namespace {
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed = 0x9747b28c) noexcept {
    uint32_t h = seed;
    const size_t nblocks = len / 4;
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(key);

    for (size_t i = 0; i < nblocks; ++i) {
        uint32_t k = blocks[i];
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;

        h ^= k;
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    const uint8_t* tail = key + (nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= static_cast<uint32_t>(tail[2]) << 16; [[fallthrough]];
        case 2: k1 ^= static_cast<uint32_t>(tail[1]) << 8;  [[fallthrough]];
        case 1: k1 ^= static_cast<uint32_t>(tail[0]);
                k1 *= 0xcc9e2d51;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= 0x1b873593;
                h ^= k1;
    }

    h ^= static_cast<uint32_t>(len);
    h ^= (h >> 16);
    h *= 0x85ebca6b;
    h ^= (h >> 13);
    h *= 0xc2b2ae35;
    h ^= (h >> 16);

    return h;
}
} // namespace

MeshGossipProtocol::MeshGossipProtocol(uint32_t localNodeId)
    : localNodeId_(localNodeId) {}

uint32_t MeshGossipProtocol::hashMessage(const std::string& messageId) noexcept {
    return murmur3_32(reinterpret_cast<const uint8_t*>(messageId.data()), messageId.size());
}

void MeshGossipProtocol::recordMessage(uint32_t messageHash, uint16_t sequenceNumber, uint8_t priority) {
    localInventory_[messageHash] = {messageHash, sequenceNumber, priority};
}

GossipDigest MeshGossipProtocol::generateLocalDigest(size_t maxEntries) const {
    GossipDigest digest;
    digest.peerNodeId = localNodeId_;
    digest.digestChecksum = 0;

    std::vector<GossipDigestEntry> sortedEntries;
    sortedEntries.reserve(localInventory_.size());
    for (const auto& [_, entry] : localInventory_) {
        sortedEntries.push_back(entry);
    }

    // Sort by priority descending (critical SOS first)
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const GossipDigestEntry& a, const GossipDigestEntry& b) {
        if (a.priority != b.priority) {
            return a.priority > b.priority;
        }
        return a.sequenceNumber > b.sequenceNumber;
    });

    if (sortedEntries.size() > maxEntries) {
        sortedEntries.resize(maxEntries);
    }

    uint32_t checksum = 0;
    for (const auto& entry : sortedEntries) {
        checksum ^= entry.messageHash;
        checksum = (checksum << 5) | (checksum >> 27);
    }

    digest.digestChecksum = checksum;
    digest.entries = std::move(sortedEntries);
    return digest;
}

ReconciliationDelta MeshGossipProtocol::reconcileWithPeer(const GossipDigest& peerDigest) const {
    ReconciliationDelta delta;
    std::unordered_set<uint32_t> peerHashes;
    peerHashes.reserve(peerDigest.entries.size());

    // Check what peer has that we might be missing
    for (const auto& pEntry : peerDigest.entries) {
        peerHashes.insert(pEntry.messageHash);
        if (localInventory_.find(pEntry.messageHash) == localInventory_.end()) {
            delta.missingMessageHashes.push_back(pEntry.messageHash); // REQUEST
        }
    }

    // Check what we have that peer is missing
    for (const auto& [hash, entry] : localInventory_) {
        if (peerHashes.find(hash) == peerHashes.end()) {
            delta.offeredMessageHashes.push_back(hash); // PUSH
        }
    }

    return delta;
}

BatteryDutyCycleConfig MeshGossipProtocol::getDutyCycleForBattery(int batteryPercentage) noexcept {
    if (batteryPercentage >= 50) {
        // High duty cycle: Scan 1000ms every 1200ms, advertise every 250ms
        return {
            .scanDurationMs = 1000,
            .scanIntervalMs = 1200,
            .advertiseIntervalMs = 250,
            .enableRelay = true
        };
    } else if (batteryPercentage >= 20) {
        // Balanced: Scan 1500ms every 5000ms, advertise every 500ms
        return {
            .scanDurationMs = 1500,
            .scanIntervalMs = 5000,
            .advertiseIntervalMs = 500,
            .enableRelay = true
        };
    } else if (batteryPercentage >= 10) {
        // Low Power: Scan 1000ms every 15000ms, advertise every 2000ms, disable relaying others
        return {
            .scanDurationMs = 1000,
            .scanIntervalMs = 15000,
            .advertiseIntervalMs = 2000,
            .enableRelay = false
        };
    } else {
        // Critical Survival: Minimal 500ms scan every 60s, advertise beacon every 5000ms
        return {
            .scanDurationMs = 500,
            .scanIntervalMs = 60000,
            .advertiseIntervalMs = 5000,
            .enableRelay = false
        };
    }
}

} // namespace georescux::mesh
