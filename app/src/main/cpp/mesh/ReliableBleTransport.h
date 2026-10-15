#ifndef GEORESCUX_RELIABLE_BLE_TRANSPORT_H
#define GEORESCUX_RELIABLE_BLE_TRANSPORT_H

#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <chrono>

namespace georescux::mesh {

enum class TransportPacketType : uint8_t {
    DATA = 0x1,
    ACK = 0x2,
    SYN = 0x3,
    FIN = 0x4
};

struct ReliablePacketHeader {
    TransportPacketType type;
    uint16_t sequenceNumber;
    uint16_t ackNumber;
    uint32_t sackBitmask; // Selective ACK bitmask (32 packets ahead of ackNumber)
    uint8_t payloadLength;
};

struct OutboundPacket {
    ReliablePacketHeader header;
    std::vector<uint8_t> payload;
    uint64_t sentTimestampMs;
    uint32_t retransmitCount;
    bool isAcknowledged;
};

/**
 * Selective-Repeat ARQ Reliable Transport Layer over Lossy BLE Advertisements.
 * Implements Jacobson/Karels adaptive RTT/RTO estimation, SACK bitmasks,
 * and out-of-order reassembly buffer.
 */
class ReliableBleTransport {
public:
    ReliableBleTransport(uint16_t windowSize = 16);

    /**
     * Queues an application payload for reliable transmission.
     * Returns sequence number assigned to this packet.
     */
    uint16_t sendPayload(const std::vector<uint8_t>& payload);

    /**
     * Ingests a raw received packet from BLE radio.
     * If an in-order sequence of payloads becomes complete, returns them in order.
     */
    std::vector<std::vector<uint8_t>> onPacketReceived(const uint8_t* data, size_t length);

    /**
     * Checks for timeouts in the outbound flight window.
     * Returns list of packets that need immediate retransmission.
     */
    std::vector<std::vector<uint8_t>> checkRetransmissions(uint64_t currentTimestampMs);

    /**
     * Creates an ACK packet to send over BLE with current cumulative ACK and SACK bitmask.
     */
    [[nodiscard]] std::vector<uint8_t> createAckPacket() const;

    /**
     * Serializes a packet into bytes for BLE advertisement broadcasting.
     */
    static std::vector<uint8_t> serializePacket(
        const ReliablePacketHeader& header,
        const std::vector<uint8_t>& payload
    );

    /**
     * Deserializes bytes into a ReliablePacketHeader and payload.
     */
    static bool deserializePacket(
        const uint8_t* data,
        size_t length,
        ReliablePacketHeader& outHeader,
        std::vector<uint8_t>& outPayload
    );

    [[nodiscard]] uint32_t getEstimatedRtoMs() const noexcept { return currentRtoMs_; }
    [[nodiscard]] size_t getUnackedPacketCount() const noexcept;

private:
    uint16_t windowSize_;
    uint16_t nextSeqNum_ = 1;
    uint16_t expectedRecvSeqNum_ = 1;

    // Sender state
    std::map<uint16_t, OutboundPacket> flightWindow_;

    // Receiver state: out-of-order buffer
    std::map<uint16_t, std::vector<uint8_t>> receiveBuffer_;

    // Jacobson/Karels RTT/RTO tracking
    double smoothedRttMs_ = 1000.0;
    double rttVarianceMs_ = 250.0;
    uint32_t currentRtoMs_ = 1000;

    void updateRttEstimate(double measuredRttMs);
    void processAck(uint16_t ackNum, uint32_t sackMask, uint64_t nowMs);
    [[nodiscard]] uint32_t buildSackBitmask() const noexcept;
};

} // namespace georescux::mesh

#endif // GEORESCUX_RELIABLE_BLE_TRANSPORT_H
