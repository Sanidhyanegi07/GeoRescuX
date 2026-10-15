#include "MeshPacketCodec.h"
#include <cstring>
#include <cmath>

namespace georescux::mesh {

uint32_t MeshPacketCodec::hashNodeId(const std::string& nodeId) noexcept {
    // 32-bit FNV-1a hash
    uint32_t hash = 2166136261u;
    for (char c : nodeId) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

uint16_t MeshPacketCodec::calculateCrc16(const uint8_t* data, size_t length) noexcept {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

std::vector<uint8_t> MeshPacketCodec::encodeSosPacket(
    uint8_t priority,
    uint8_t ttl,
    uint16_t seqNum,
    const std::string& senderId,
    double lat, double lon, double altMeters,
    uint8_t batteryPct,
    uint8_t urgency,
    uint16_t peopleCount,
    uint32_t timestampSec
) {
    MeshPacketHeader header{};
    header.protocolVersion = CURRENT_PROTOCOL_VERSION;
    header.messageType = static_cast<uint8_t>(MeshMessageType::SOS_DISTRESS);
    header.priority = priority & 0x03;
    header.ttl = ttl & 0x3F;
    header.sequenceNumber = seqNum;
    header.senderIdHash = hashNodeId(senderId);
    header.recipientIdHash = BROADCAST_RECIPIENT_HASH;
    header.timestampEpochSec = timestampSec;

    SosPayload payload{};
    payload.latFixedPoint = static_cast<int32_t>(std::round(lat * 1e7));
    payload.lonFixedPoint = static_cast<int32_t>(std::round(lon * 1e7));
    payload.altitudeMeters = static_cast<int16_t>(std::clamp(altMeters, -1000.0, 9000.0));
    payload.batteryPercent = batteryPct;
    payload.medicalUrgency = urgency;
    payload.peopleCount = peopleCount;

    const size_t totalBytes = sizeof(MeshPacketHeader) + sizeof(SosPayload) + sizeof(uint16_t);
    std::vector<uint8_t> buffer(totalBytes);

    std::memcpy(buffer.data(), &header, sizeof(MeshPacketHeader));
    std::memcpy(buffer.data() + sizeof(MeshPacketHeader), &payload, sizeof(SosPayload));

    // Compute CRC-16 over Header + Payload
    const uint16_t crc = calculateCrc16(buffer.data(), sizeof(MeshPacketHeader) + sizeof(SosPayload));
    buffer[totalBytes - 2] = static_cast<uint8_t>(crc >> 8);
    buffer[totalBytes - 1] = static_cast<uint8_t>(crc & 0xFF);

    return buffer;
}

std::optional<DecodedMeshPacket> MeshPacketCodec::decodePacket(const uint8_t* data, size_t length) {
    if (!data || length < MIN_PACKET_SIZE) return std::nullopt;

    // Verify CRC-16
    const uint16_t expectedCrc = (static_cast<uint16_t>(data[length - 2]) << 8) | data[length - 1];
    const uint16_t computedCrc = calculateCrc16(data, length - 2);
    if (expectedCrc != computedCrc) return std::nullopt;

    MeshPacketHeader header{};
    std::memcpy(&header, data, sizeof(MeshPacketHeader));

    if (header.protocolVersion != CURRENT_PROTOCOL_VERSION) {
        return std::nullopt;
    }

    DecodedMeshPacket decoded{};
    decoded.messageType = static_cast<MeshMessageType>(header.messageType);
    decoded.priority = static_cast<MeshPriority>(header.priority);
    decoded.ttl = header.ttl;
    decoded.sequenceNumber = header.sequenceNumber;
    decoded.senderIdHash = header.senderIdHash;
    decoded.recipientIdHash = header.recipientIdHash;
    decoded.timestampEpochSec = header.timestampEpochSec;

    const size_t payloadLen = length - sizeof(MeshPacketHeader) - 2;
    const uint8_t* payloadPtr = data + sizeof(MeshPacketHeader);

    if (decoded.messageType == MeshMessageType::SOS_DISTRESS && payloadLen >= sizeof(SosPayload)) {
        SosPayload sos{};
        std::memcpy(&sos, payloadPtr, sizeof(SosPayload));
        decoded.hasSosPayload = true;
        decoded.latitude = static_cast<double>(sos.latFixedPoint) / 1e7;
        decoded.longitude = static_cast<double>(sos.lonFixedPoint) / 1e7;
        decoded.altitudeMeters = static_cast<double>(sos.altitudeMeters);
        decoded.batteryPercent = sos.batteryPercent;
        decoded.medicalUrgency = sos.medicalUrgency;
        decoded.peopleCount = sos.peopleCount;
    } else if (payloadLen > 0) {
        decoded.customPayload.assign(payloadPtr, payloadPtr + payloadLen);
    }

    return decoded;
}

} // namespace georescux::mesh
