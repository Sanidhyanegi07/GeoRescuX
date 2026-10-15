#ifndef GEORESCUX_MESH_PACKET_CODEC_H
#define GEORESCUX_MESH_PACKET_CODEC_H

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace georescux::mesh {

enum class MeshMessageType : uint8_t {
    SOS_DISTRESS = 0x01,
    HEARTBEAT_STATUS = 0x02,
    SAFE_HAVEN_ANNOUNCEMENT = 0x03,
    HAZARD_WARNING = 0x04,
    RELAY_ACK = 0x05,
    TRIAGE_REPORT = 0x06
};

enum class MeshPriority : uint8_t {
    CRITICAL = 0x03,
    HIGH = 0x02,
    NORMAL = 0x01,
    BACKGROUND = 0x00
};

#pragma pack(push, 1)
struct MeshPacketHeader {
    uint8_t protocolVersion : 4;  // Protocol version (e.g. 1)
    uint8_t messageType : 4;      // MeshMessageType
    uint8_t priority : 2;         // MeshPriority
    uint8_t ttl : 6;              // Time-to-live hops (0-63)
    uint16_t sequenceNumber;      // Monotonic sequence number
    uint32_t senderIdHash;        // 32-bit FNV-1a hash of sender node ID
    uint32_t recipientIdHash;     // 32-bit FNV-1a hash (0xFFFFFFFF = broadcast)
    uint32_t timestampEpochSec;   // UTC timestamp in seconds
};

struct SosPayload {
    int32_t latFixedPoint;        // lat * 1e7
    int32_t lonFixedPoint;        // lon * 1e7
    int16_t altitudeMeters;       // altitude in meters
    uint8_t batteryPercent;       // 0 - 100%
    uint8_t medicalUrgency;       // 0 = uninjured, 1 = minor, 2 = delayed, 3 = immediate
    uint16_t peopleCount;         // Number of people requiring evacuation
};
#pragma pack(pop)

struct DecodedMeshPacket {
    MeshMessageType messageType;
    MeshPriority priority;
    uint8_t ttl;
    uint16_t sequenceNumber;
    uint32_t senderIdHash;
    uint32_t recipientIdHash;
    uint32_t timestampEpochSec;

    // Optional SOS payload
    bool hasSosPayload{false};
    double latitude{0.0};
    double longitude{0.0};
    double altitudeMeters{0.0};
    uint8_t batteryPercent{0};
    uint8_t medicalUrgency{0};
    uint16_t peopleCount{0};

    // Arbitrary binary payload if non-SOS
    std::vector<uint8_t> customPayload;
};

/**
 * High-performance bit-packed binary codec for BLE mesh packets.
 * Encodes packets into 20-31 byte standard payloads with CRC-16-CCITT integrity checks.
 */
class MeshPacketCodec {
public:
    static constexpr uint8_t CURRENT_PROTOCOL_VERSION = 1;
    static constexpr uint32_t BROADCAST_RECIPIENT_HASH = 0xFFFFFFFF;
    static constexpr size_t MIN_PACKET_SIZE = sizeof(MeshPacketHeader) + 2; // header + 2-byte CRC

    /**
     * Encodes an SOS distress packet into a compact binary buffer.
     */
    static std::vector<uint8_t> encodeSosPacket(
        uint8_t priority,
        uint8_t ttl,
        uint16_t seqNum,
        const std::string& senderId,
        double lat, double lon, double altMeters,
        uint8_t batteryPct,
        uint8_t urgency,
        uint16_t peopleCount,
        uint32_t timestampSec
    );

    /**
     * Decodes an incoming binary mesh packet.
     * Validates CRC-16 and protocol version. Returns nullopt if corrupt or invalid.
     */
    static std::optional<DecodedMeshPacket> decodePacket(const uint8_t* data, size_t length);

    /**
     * Calculates 16-bit CRC-CCITT (polynomial 0x1021, init 0xFFFF).
     */
    static uint16_t calculateCrc16(const uint8_t* data, size_t length) noexcept;

    /**
     * Fast 32-bit FNV-1a hash for node string IDs.
     */
    static uint32_t hashNodeId(const std::string& nodeId) noexcept;
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_PACKET_CODEC_H
