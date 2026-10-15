#include "BloomFilterDeduplicator.h"
#include <algorithm>

namespace georescux::mesh {

BloomFilterDeduplicator::BloomFilterDeduplicator(
    size_t bitCount,
    size_t numHashes,
    int64_t epochDurationSeconds
) : bitCount_(std::max<size_t>(1024, bitCount)),
    numHashes_(std::clamp<size_t>(numHashes, 1, 16)),
    epochDurationSeconds_(std::max<int64_t>(10, epochDurationSeconds)),
    lastRotationTime_(std::chrono::steady_clock::now()) {

    const size_t words = (bitCount_ + 63) / 64;
    activeBuffer_.resize(words, 0);
    previousBuffer_.resize(words, 0);
}

void BloomFilterDeduplicator::reset() {
    std::fill(activeBuffer_.begin(), activeBuffer_.end(), 0);
    std::fill(previousBuffer_.begin(), previousBuffer_.end(), 0);
    currentItemCount_ = 0;
    lastRotationTime_ = std::chrono::steady_clock::now();
}

void BloomFilterDeduplicator::rotateEpoch() {
    previousBuffer_ = std::move(activeBuffer_);
    activeBuffer_.assign((bitCount_ + 63) / 64, 0);
    currentItemCount_ = 0;
    lastRotationTime_ = std::chrono::steady_clock::now();
}

void BloomFilterDeduplicator::checkEpochExpiration() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRotationTime_).count();
    if (elapsed >= epochDurationSeconds_) {
        rotateEpoch();
    }
}

void BloomFilterDeduplicator::computeHashes(uint64_t key, std::vector<size_t>& indices) const {
    indices.resize(numHashes_);
    // Kirsch-Mitzenmacher optimization: g_i(x) = h1(x) + i * h2(x)
    // 64-bit Murmur/Mix hash for h1 and h2
    uint64_t h1 = key ^ 0x517cc1b727220a95ULL;
    h1 = (h1 ^ (h1 >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h1 = (h1 ^ (h1 >> 27)) * 0x94d049bb133111ebULL;
    h1 = h1 ^ (h1 >> 31);

    uint64_t h2 = key ^ 0x6a09e667f3bcc908ULL;
    h2 = (h2 ^ (h2 >> 30)) * 0xdc3a7d4576322fcdULL;
    h2 = (h2 ^ (h2 >> 27)) * 0x7a879d034b071735ULL;
    h2 = h2 ^ (h2 >> 31);
    if (h2 == 0) h2 = 1;

    for (size_t i = 0; i < numHashes_; ++i) {
        indices[i] = (h1 + i * h2) % bitCount_;
    }
}

bool BloomFilterDeduplicator::checkAndMark(uint32_t senderHash, uint16_t sequenceNumber) {
    checkEpochExpiration();

    const uint64_t key = (static_cast<uint64_t>(senderHash) << 32) | sequenceNumber;
    std::vector<size_t> indices;
    computeHashes(key, indices);

    // 1. Check if present in active buffer
    bool inActive = true;
    for (size_t idx : indices) {
        if (!testBit(activeBuffer_, idx)) {
            inActive = false;
            break;
        }
    }
    if (inActive) return true; // Definite or probabilistic match in current epoch

    // 2. Check if present in previous buffer
    bool inPrevious = true;
    for (size_t idx : indices) {
        if (!testBit(previousBuffer_, idx)) {
            inPrevious = false;
            break;
        }
    }
    if (inPrevious) {
        // Also promote to active buffer so it stays active
        for (size_t idx : indices) {
            setBit(activeBuffer_, idx);
        }
        return true;
    }

    // 3. New item: mark in active buffer
    for (size_t idx : indices) {
        setBit(activeBuffer_, idx);
    }
    currentItemCount_++;

    return false; // Not a duplicate
}

bool BloomFilterDeduplicator::checkAndMark(const std::string& packetIdentifier) {
    // 64-bit FNV-1a hash of string
    uint64_t hash = 14695981039346656037ULL;
    for (char c : packetIdentifier) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    const uint32_t sender = static_cast<uint32_t>(hash >> 32);
    const uint16_t seq = static_cast<uint16_t>(hash & 0xFFFF);
    return checkAndMark(sender, seq);
}

} // namespace georescux::mesh
