#include "ReliableBleTransport.h"
#include <algorithm>
#include <cmath>

namespace georescux::mesh {

namespace {
uint64_t getCurrentTimeMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}
} // namespace

ReliableBleTransport::ReliableBleTransport(uint16_t windowSize)
    : windowSize_(windowSize) {}

std::vector<uint8_t> ReliableBleTransport::serializePacket(
    const ReliablePacketHeader& header,
    const std::vector<uint8_t>& payload
) {
    std::vector<uint8_t> buffer;
    buffer.reserve(10 + payload.size());

    buffer.push_back(static_cast<uint8_t>(header.type));
    buffer.push_back(static_cast<uint8_t>((header.sequenceNumber >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(header.sequenceNumber & 0xFF));
    buffer.push_back(static_cast<uint8_t>((header.ackNumber >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(header.ackNumber & 0xFF));

    buffer.push_back(static_cast<uint8_t>((header.sackBitmask >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((header.sackBitmask >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((header.sackBitmask >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(header.sackBitmask & 0xFF));

    uint8_t payloadLen = static_cast<uint8_t>(std::min(payload.size(), static_cast<size_t>(240)));
    buffer.push_back(payloadLen);

    buffer.insert(buffer.end(), payload.begin(), payload.begin() + payloadLen);
    return buffer;
}

bool ReliableBleTransport::deserializePacket(
    const uint8_t* data,
    size_t length,
    ReliablePacketHeader& outHeader,
    std::vector<uint8_t>& outPayload
) {
    if (data == nullptr || length < 10) return false;

    outHeader.type = static_cast<TransportPacketType>(data[0]);
    outHeader.sequenceNumber = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);
    outHeader.ackNumber = (static_cast<uint16_t>(data[3]) << 8) | static_cast<uint16_t>(data[4]);

    outHeader.sackBitmask = (static_cast<uint32_t>(data[5]) << 24) |
                            (static_cast<uint32_t>(data[6]) << 16) |
                            (static_cast<uint32_t>(data[7]) << 8) |
                            static_cast<uint32_t>(data[8]);

    outHeader.payloadLength = data[9];
    if (length < 10 + outHeader.payloadLength) {
        return false;
    }

    outPayload.assign(data + 10, data + 10 + outHeader.payloadLength);
    return true;
}

uint16_t ReliableBleTransport::sendPayload(const std::vector<uint8_t>& payload) {
    uint16_t seq = nextSeqNum_++;
    uint64_t now = getCurrentTimeMs();

    ReliablePacketHeader header;
    header.type = TransportPacketType::DATA;
    header.sequenceNumber = seq;
    header.ackNumber = expectedRecvSeqNum_ - 1;
    header.sackBitmask = buildSackBitmask();
    header.payloadLength = static_cast<uint8_t>(std::min(payload.size(), static_cast<size_t>(240)));

    flightWindow_[seq] = OutboundPacket{
        header,
        payload,
        now,
        0,     // retransmitCount
        false  // isAcknowledged
    };

    return seq;
}

void ReliableBleTransport::updateRttEstimate(double measuredRttMs) {
    // Jacobson/Karels algorithm (RFC 6298)
    double err = measuredRttMs - smoothedRttMs_;
    smoothedRttMs_ += 0.125 * err;
    rttVarianceMs_ += 0.25 * (std::abs(err) - rttVarianceMs_);
    currentRtoMs_ = static_cast<uint32_t>(std::clamp(smoothedRttMs_ + 4.0 * rttVarianceMs_, 200.0, 5000.0));
}

void ReliableBleTransport::processAck(uint16_t ackNum, uint32_t sackMask, uint64_t nowMs) {
    // Cumulative ACK: removes all packets with seq <= ackNum
    auto it = flightWindow_.begin();
    while (it != flightWindow_.end()) {
        if (it->first <= ackNum) {
            if (it->second.retransmitCount == 0) {
                double rtt = static_cast<double>(nowMs - it->second.sentTimestampMs);
                updateRttEstimate(rtt);
            }
            it = flightWindow_.erase(it);
        } else {
            // SACK check: bit i corresponds to ackNum + 1 + i
            uint16_t offset = it->first - (ackNum + 1);
            if (offset < 32 && ((sackMask >> offset) & 1)) {
                it->second.isAcknowledged = true;
            }
            ++it;
        }
    }
}

uint32_t ReliableBleTransport::buildSackBitmask() const noexcept {
    uint32_t mask = 0;
    for (uint16_t offset = 0; offset < 32; ++offset) {
        uint16_t targetSeq = expectedRecvSeqNum_ + 1 + offset;
        if (receiveBuffer_.find(targetSeq) != receiveBuffer_.end()) {
            mask |= (1U << offset);
        }
    }
    return mask;
}

std::vector<std::vector<uint8_t>> ReliableBleTransport::onPacketReceived(const uint8_t* data, size_t length) {
    std::vector<std::vector<uint8_t>> delivered;
    ReliablePacketHeader header;
    std::vector<uint8_t> payload;

    if (!deserializePacket(data, length, header, payload)) {
        return delivered;
    }

    uint64_t now = getCurrentTimeMs();
    processAck(header.ackNumber, header.sackBitmask, now);

    if (header.type == TransportPacketType::DATA) {
        uint16_t seq = header.sequenceNumber;

        if (seq == expectedRecvSeqNum_) {
            delivered.push_back(payload);
            expectedRecvSeqNum_++;

            // Check if subsequent out-of-order packets can now be delivered in-order
            auto it = receiveBuffer_.begin();
            while (it != receiveBuffer_.end() && it->first == expectedRecvSeqNum_) {
                delivered.push_back(it->second);
                expectedRecvSeqNum_++;
                it = receiveBuffer_.erase(it);
            }
        } else if (seq > expectedRecvSeqNum_) {
            // Out of order packet within reasonable window
            if (seq < expectedRecvSeqNum_ + windowSize_) {
                receiveBuffer_[seq] = payload;
            }
        }
    }

    return delivered;
}

std::vector<std::vector<uint8_t>> ReliableBleTransport::checkRetransmissions(uint64_t currentTimestampMs) {
    std::vector<std::vector<uint8_t>> packetsToRetransmit;

    for (auto& [seq, packet] : flightWindow_) {
        if (packet.isAcknowledged) continue;

        uint64_t elapsed = currentTimestampMs - packet.sentTimestampMs;
        uint32_t effectiveRto = currentRtoMs_ * (1U << std::min(packet.retransmitCount, 4U));

        if (elapsed >= effectiveRto) {
            // Retransmit
            packet.sentTimestampMs = currentTimestampMs;
            packet.retransmitCount++;
            packet.header.ackNumber = expectedRecvSeqNum_ - 1;
            packet.header.sackBitmask = buildSackBitmask();

            packetsToRetransmit.push_back(serializePacket(packet.header, packet.payload));
        }
    }

    return packetsToRetransmit;
}

std::vector<uint8_t> ReliableBleTransport::createAckPacket() const {
    ReliablePacketHeader ack;
    ack.type = TransportPacketType::ACK;
    ack.sequenceNumber = 0;
    ack.ackNumber = expectedRecvSeqNum_ - 1;
    ack.sackBitmask = buildSackBitmask();
    ack.payloadLength = 0;
    return serializePacket(ack, {});
}

size_t ReliableBleTransport::getUnackedPacketCount() const noexcept {
    size_t count = 0;
    for (const auto& [_, p] : flightWindow_) {
        if (!p.isAcknowledged) count++;
    }
    return count;
}

} // namespace georescux::mesh
