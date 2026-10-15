#include "PacketFragmenter.h"
#include <cstring>
#include <algorithm>

namespace georescux::mesh {

namespace {
uint8_t computeChecksum(const uint8_t* data, size_t len) noexcept {
    uint8_t c = 0;
    for (size_t i = 0; i < len; ++i) {
        c ^= data[i];
    }
    return c;
}
} // namespace

std::vector<std::vector<uint8_t>> PacketFragmenter::fragment(
    uint16_t messageId,
    const uint8_t* payload,
    size_t payloadLength,
    size_t maxFragmentPayload
) {
    std::vector<std::vector<uint8_t>> result;
    if (!payload || payloadLength == 0) return result;

    const size_t chunkSize = std::max<size_t>(1, maxFragmentPayload);
    const size_t totalFrags = (payloadLength + chunkSize - 1) / chunkSize;

    if (totalFrags > 16) {
        return result; // Exceeds 4-bit fragment indexing limit
    }

    result.reserve(totalFrags);

    for (size_t idx = 0; idx < totalFrags; ++idx) {
        const size_t offset = idx * chunkSize;
        const size_t currentChunk = std::min(chunkSize, payloadLength - offset);

        FragmentHeader hdr{};
        hdr.messageId = messageId;
        hdr.fragmentIndex = static_cast<uint8_t>(idx);
        hdr.totalFragments = static_cast<uint8_t>(totalFrags);
        hdr.checksum = computeChecksum(payload + offset, currentChunk);

        std::vector<uint8_t> packet(sizeof(FragmentHeader) + currentChunk);
        std::memcpy(packet.data(), &hdr, sizeof(FragmentHeader));
        std::memcpy(packet.data() + sizeof(FragmentHeader), payload + offset, currentChunk);

        result.push_back(std::move(packet));
    }

    return result;
}

PacketFragmenter::Reassembler::Reassembler(int64_t timeoutSeconds)
    : timeoutSeconds_(std::max<int64_t>(5, timeoutSeconds)) {}

std::optional<std::vector<uint8_t>> PacketFragmenter::Reassembler::feedFragment(
    const uint8_t* data, size_t length
) {
    if (!data || length <= sizeof(FragmentHeader)) return std::nullopt;

    pruneTimeouts();

    FragmentHeader hdr{};
    std::memcpy(&hdr, data, sizeof(FragmentHeader));

    const size_t payloadLen = length - sizeof(FragmentHeader);
    const uint8_t* payloadPtr = data + sizeof(FragmentHeader);

    if (computeChecksum(payloadPtr, payloadLen) != hdr.checksum) {
        return std::nullopt; // Corrupt fragment
    }

    if (hdr.fragmentIndex >= hdr.totalFragments || hdr.totalFragments == 0 || hdr.totalFragments > 16) {
        return std::nullopt; // Invalid header
    }

    auto& pending = pending_[hdr.messageId];
    if (pending.totalFragments == 0) {
        pending.totalFragments = hdr.totalFragments;
        pending.fragments.resize(hdr.totalFragments);
        pending.received.assign(hdr.totalFragments, false);
        pending.firstReceivedTime = std::chrono::steady_clock::now();
    }

    if (pending.totalFragments != hdr.totalFragments) {
        return std::nullopt; // Mismatched fragment total
    }

    if (!pending.received[hdr.fragmentIndex]) {
        pending.fragments[hdr.fragmentIndex].assign(payloadPtr, payloadPtr + payloadLen);
        pending.received[hdr.fragmentIndex] = true;
    }

    // Check if complete
    bool allReceived = true;
    size_t totalBytes = 0;
    for (size_t i = 0; i < pending.totalFragments; ++i) {
        if (!pending.received[i]) {
            allReceived = false;
            break;
        }
        totalBytes += pending.fragments[i].size();
    }

    if (allReceived) {
        std::vector<uint8_t> fullMessage;
        fullMessage.reserve(totalBytes);
        for (const auto& frag : pending.fragments) {
            fullMessage.insert(fullMessage.end(), frag.begin(), frag.end());
        }
        pending_.erase(hdr.messageId);
        return fullMessage;
    }

    return std::nullopt;
}

void PacketFragmenter::Reassembler::pruneTimeouts() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = pending_.begin(); it != pending_.end(); ) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.firstReceivedTime).count();
        if (elapsed > timeoutSeconds_) {
            it = pending_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace georescux::mesh
