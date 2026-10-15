#ifndef GEORESCUX_MESH_GOSSIP_PROTOCOL_H
#define GEORESCUX_MESH_GOSSIP_PROTOCOL_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <memory>

namespace georescux::mesh {

struct GossipDigestEntry {
    uint32_t messageHash;
    uint16_t sequenceNumber;
    uint8_t priority;
};

struct GossipDigest {
    uint32_t peerNodeId;
    uint32_t digestChecksum;
    std::vector<GossipDigestEntry> entries;
};

struct ReconciliationDelta {
    std::vector<uint32_t> missingMessageHashes; // Hashes peer has, but we lack (REQUEST)
    std::vector<uint32_t> offeredMessageHashes; // Hashes we have, but peer lacks (PUSH)
};

enum class PowerMode : uint8_t {
    FULL_POWER = 1,      // 100% - 50% battery: Active scanning & advertising
    BALANCED = 2,        // 50% - 20% battery: Intermittent 5s sleep cycles
    LOW_BATTERY = 3,     // 20% - 10% battery: 15s sleep cycles, SOS only
    CRITICAL_SURVIVAL = 4 // < 10% battery: 60s sleep cycles, minimal beacon
};

struct BatteryDutyCycleConfig {
    uint32_t scanDurationMs;
    uint32_t scanIntervalMs;
    uint32_t advertiseIntervalMs;
    bool enableRelay;
};

/**
 * Anti-Entropy Push-Pull Gossip Synchronization Protocol for Disaster Mesh Networks.
 * Handles intermittent opportunistic synchronization between moving rescue workers
 * and optimizes battery consumption dynamically based on battery level.
 */
class MeshGossipProtocol {
public:
    MeshGossipProtocol(uint32_t localNodeId);

    /**
     * Records a local message in the gossip inventory.
     */
    void recordMessage(uint32_t messageHash, uint16_t sequenceNumber, uint8_t priority);

    /**
     * Generates a compact digest of local message state to send to an encountered peer.
     */
    [[nodiscard]] GossipDigest generateLocalDigest(size_t maxEntries = 64) const;

    /**
     * Compares an incoming peer digest against local inventory to calculate
     * missing messages to pull, and local messages to push.
     */
    [[nodiscard]] ReconciliationDelta reconcileWithPeer(const GossipDigest& peerDigest) const;

    /**
     * Calculates duty-cycle parameters based on current battery percentage (0 - 100).
     */
    static BatteryDutyCycleConfig getDutyCycleForBattery(int batteryPercentage) noexcept;

    /**
     * Calculates 32-bit MurmurHash3 for message payload / ID.
     */
    static uint32_t hashMessage(const std::string& messageId) noexcept;

    [[nodiscard]] size_t getKnownMessageCount() const noexcept { return localInventory_.size(); }

private:
    uint32_t localNodeId_;
    std::unordered_map<uint32_t, GossipDigestEntry> localInventory_;
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_GOSSIP_PROTOCOL_H
