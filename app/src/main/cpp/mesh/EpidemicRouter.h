#ifndef GEORESCUX_MESH_EPIDEMIC_ROUTER_H
#define GEORESCUX_MESH_EPIDEMIC_ROUTER_H

#include "MeshPacketCodec.h"
#include "BloomFilterDeduplicator.h"
#include <vector>
#include <deque>
#include <string>
#include <optional>
#include <chrono>

namespace georescux::mesh {

struct QueuedPacket {
    std::vector<uint8_t> rawBytes;
    DecodedMeshPacket decoded;
    std::chrono::steady_clock::time_point receivedTime;
    uint8_t forwardCount{0};
};

/**
 * Epidemic / Delay-Tolerant Network (DTN) Router.
 * Manages store-carry-and-forward message buffer with priority queueing,
 * anti-entropy summary vector generation, and proactive buffer pruning.
 */
class EpidemicRouter {
public:
    static constexpr size_t DEFAULT_MAX_BUFFER_SIZE = 500;
    static constexpr int64_t DEFAULT_PACKET_MAX_LIFETIME_SEC = 3600; // 1 hour

    explicit EpidemicRouter(
        std::string localNodeId,
        size_t maxBufferSize = DEFAULT_MAX_BUFFER_SIZE,
        int64_t maxLifetimeSec = DEFAULT_PACKET_MAX_LIFETIME_SEC
    );

    [[nodiscard]] const std::string& getLocalNodeId() const noexcept { return localNodeId_; }
    [[nodiscard]] uint32_t getLocalNodeHash() const noexcept { return localNodeHash_; }

    /**
     * Ingests an incoming packet received over BLE.
     * Evaluates deduplication, TTL, priority, and enqueues for forwarding.
     * Returns true if packet was accepted into queue, false if dropped/duplicate.
     */
    bool ingestPacket(const uint8_t* data, size_t length);

    /**
     * Creates and enqueues a new local SOS packet generated on this device.
     */
    bool enqueueLocalSos(
        double lat, double lon, double altMeters,
        uint8_t batteryPct, uint8_t urgency, uint16_t peopleCount,
        uint32_t timestampSec
    );

    /**
     * Gets a batch of up to maxPackets ready for broadcast transmission over BLE.
     * Decrements TTL and increments forwardCount.
     */
    std::vector<std::vector<uint8_t>> getPacketsForTransmission(size_t maxPackets = 5);

    /**
     * Generates a 32-bit summary vector of currently held packet hashes
     * for anti-entropy peer exchange.
     */
    std::vector<uint32_t> getSummaryVector() const;

    /**
     * Prunes expired packets and low-priority messages if buffer is constrained.
     */
    void pruneExpiredPackets();

    [[nodiscard]] size_t getQueueSize() const noexcept { return queue_.size(); }
    [[nodiscard]] size_t getMaxBufferSize() const noexcept { return maxBufferSize_; }

    void clear() noexcept;

private:
    std::string localNodeId_;
    uint32_t localNodeHash_;
    size_t maxBufferSize_;
    int64_t maxLifetimeSec_;
    uint16_t localSequenceNumber_{0};

    BloomFilterDeduplicator deduplicator_;
    std::deque<QueuedPacket> queue_;

    void evictLowestPriority();
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_EPIDEMIC_ROUTER_H
