#include "EpidemicRouter.h"
#include <algorithm>

namespace georescux::mesh {

EpidemicRouter::EpidemicRouter(
    std::string localNodeId,
    size_t maxBufferSize,
    int64_t maxLifetimeSec
) : localNodeId_(std::move(localNodeId)),
    localNodeHash_(MeshPacketCodec::hashNodeId(localNodeId_)),
    maxBufferSize_(std::max<size_t>(10, maxBufferSize)),
    maxLifetimeSec_(std::max<int64_t>(60, maxLifetimeSec)),
    deduplicator_(65536, 4, 300) {}

void EpidemicRouter::clear() noexcept {
    queue_.clear();
    deduplicator_.reset();
    localSequenceNumber_ = 0;
}

bool EpidemicRouter::ingestPacket(const uint8_t* data, size_t length) {
    if (!data || length < MeshPacketCodec::MIN_PACKET_SIZE) return false;

    auto decodedOpt = MeshPacketCodec::decodePacket(data, length);
    if (!decodedOpt) return false;

    auto& decoded = *decodedOpt;

    // Do not re-ingest our own packets
    if (decoded.senderIdHash == localNodeHash_) return false;

    // TTL expired
    if (decoded.ttl == 0) return false;

    // Deduplication check
    if (deduplicator_.checkAndMark(decoded.senderIdHash, decoded.sequenceNumber)) {
        return false; // Already seen or in circulation
    }

    pruneExpiredPackets();

    if (queue_.size() >= maxBufferSize_) {
        evictLowestPriority();
    }

    QueuedPacket qp;
    qp.rawBytes.assign(data, data + length);
    qp.decoded = decoded;
    qp.receivedTime = std::chrono::steady_clock::now();
    qp.forwardCount = 0;

    queue_.push_back(std::move(qp));
    return true;
}

bool EpidemicRouter::enqueueLocalSos(
    double lat, double lon, double altMeters,
    uint8_t batteryPct, uint8_t urgency, uint16_t peopleCount,
    uint32_t timestampSec
) {
    const uint16_t seq = ++localSequenceNumber_;
    const uint8_t initialTtl = 15; // 15 hops max
    const uint8_t priority = static_cast<uint8_t>(MeshPriority::CRITICAL);

    std::vector<uint8_t> encoded = MeshPacketCodec::encodeSosPacket(
        priority, initialTtl, seq, localNodeId_,
        lat, lon, altMeters, batteryPct, urgency, peopleCount, timestampSec
    );

    auto decodedOpt = MeshPacketCodec::decodePacket(encoded.data(), encoded.size());
    if (!decodedOpt) return false;

    deduplicator_.checkAndMark(localNodeHash_, seq);

    if (queue_.size() >= maxBufferSize_) {
        evictLowestPriority();
    }

    QueuedPacket qp;
    qp.rawBytes = std::move(encoded);
    qp.decoded = *decodedOpt;
    qp.receivedTime = std::chrono::steady_clock::now();
    qp.forwardCount = 0;

    // Local SOS goes straight to front of the priority line
    queue_.push_front(std::move(qp));
    return true;
}

std::vector<std::vector<uint8_t>> EpidemicRouter::getPacketsForTransmission(size_t maxPackets) {
    std::vector<std::vector<uint8_t>> batch;
    if (queue_.empty() || maxPackets == 0) return batch;

    pruneExpiredPackets();

    // Sort queue by Priority (descending) and forwardCount (ascending)
    std::sort(queue_.begin(), queue_.end(), [](const QueuedPacket& a, const QueuedPacket& b) {
        if (a.decoded.priority != b.decoded.priority) {
            return static_cast<uint8_t>(a.decoded.priority) > static_cast<uint8_t>(b.decoded.priority);
        }
        return a.forwardCount < b.forwardCount;
    });

    const size_t count = std::min(maxPackets, queue_.size());
    for (size_t i = 0; i < count; ++i) {
        auto& item = queue_[i];
        if (item.decoded.ttl > 0) {
            // Decrement TTL in raw bytes and recompute CRC
            item.decoded.ttl--;
            item.forwardCount++;

            // Update header TTL field (bits 2..7 of byte 1)
            if (item.rawBytes.size() >= sizeof(MeshPacketHeader) + 2) {
                auto* hdr = reinterpret_cast<MeshPacketHeader*>(item.rawBytes.data());
                hdr->ttl = item.decoded.ttl;
                // Recompute CRC
                const uint16_t newCrc = MeshPacketCodec::calculateCrc16(
                    item.rawBytes.data(), item.rawBytes.size() - 2
                );
                item.rawBytes[item.rawBytes.size() - 2] = static_cast<uint8_t>(newCrc >> 8);
                item.rawBytes[item.rawBytes.size() - 1] = static_cast<uint8_t>(newCrc & 0xFF);
            }

            batch.push_back(item.rawBytes);
        }
    }

    return batch;
}

std::vector<uint32_t> EpidemicRouter::getSummaryVector() const {
    std::vector<uint32_t> summary;
    summary.reserve(queue_.size());
    for (const auto& item : queue_) {
        // Hash combination of sender + seq
        const uint32_t packetHash = item.decoded.senderIdHash ^ (static_cast<uint32_t>(item.decoded.sequenceNumber) * 2654435761u);
        summary.push_back(packetHash);
    }
    return summary;
}

void EpidemicRouter::pruneExpiredPackets() {
    const auto now = std::chrono::steady_clock::now();
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(), [this, now](const QueuedPacket& p) {
            if (p.decoded.ttl == 0) return true;
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - p.receivedTime).count();
            return elapsed > maxLifetimeSec_;
        }),
        queue_.end()
    );
}

void EpidemicRouter::evictLowestPriority() {
    if (queue_.empty()) return;

    // Find the item with lowest priority and highest forward count
    auto lowestIt = queue_.begin();
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        if (static_cast<uint8_t>(it->decoded.priority) < static_cast<uint8_t>(lowestIt->decoded.priority)) {
            lowestIt = it;
        } else if (it->decoded.priority == lowestIt->decoded.priority && it->forwardCount > lowestIt->forwardCount) {
            lowestIt = it;
        }
    }

    queue_.erase(lowestIt);
}

} // namespace georescux::mesh
