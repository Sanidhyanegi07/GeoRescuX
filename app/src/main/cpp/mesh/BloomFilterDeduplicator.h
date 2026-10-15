#ifndef GEORESCUX_MESH_BLOOM_FILTER_DEDUPLICATOR_H
#define GEORESCUX_MESH_BLOOM_FILTER_DEDUPLICATOR_H

#include <vector>
#include <cstdint>
#include <string>
#include <chrono>

namespace georescux::mesh {

/**
 * Double-buffered rolling Bloom filter for ultra-fast, zero-allocation
 * packet deduplication in high-density BLE disaster mesh environments.
 * Prevents broadcast packet storms while consuming less than 64KB RAM.
 */
class BloomFilterDeduplicator {
public:
    /**
     * @param bitCount Number of bits per buffer (default: 65,536 bits = 8KB)
     * @param numHashes Number of independent hash functions (default: 4)
     * @param epochDurationSeconds Time before rotating active buffer (default: 300s = 5 min)
     */
    explicit BloomFilterDeduplicator(
        size_t bitCount = 65536,
        size_t numHashes = 4,
        int64_t epochDurationSeconds = 300
    );

    /**
     * Checks if a packet has already been seen in the current or previous epoch.
     * If not seen, automatically marks it as seen.
     * Returns true if packet is a DUPLICATE, false if packet is NEW.
     */
    bool checkAndMark(uint32_t senderHash, uint16_t sequenceNumber);

    /**
     * Convenience method for string-based packet identification.
     */
    bool checkAndMark(const std::string& packetIdentifier);

    /**
     * Explicitly forces an epoch rotation (clearing the oldest buffer).
     */
    void rotateEpoch();

    /**
     * Clears all filters.
     */
    void reset();

    /**
     * Approximate count of items stored in current epoch.
     */
    [[nodiscard]] size_t getCurrentEpochItemCount() const noexcept { return currentItemCount_; }

private:
    size_t bitCount_;
    size_t numHashes_;
    int64_t epochDurationSeconds_;
    std::chrono::steady_clock::time_point lastRotationTime_;

    std::vector<uint64_t> activeBuffer_;
    std::vector<uint64_t> previousBuffer_;
    size_t currentItemCount_{0};

    void checkEpochExpiration();
    void computeHashes(uint64_t key, std::vector<size_t>& indices) const;

    static inline void setBit(std::vector<uint64_t>& buf, size_t bitIdx) noexcept {
        buf[bitIdx / 64] |= (1ULL << (bitIdx % 64));
    }

    static inline bool testBit(const std::vector<uint64_t>& buf, size_t bitIdx) noexcept {
        return (buf[bitIdx / 64] & (1ULL << (bitIdx % 64))) != 0;
    }
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_BLOOM_FILTER_DEDUPLICATOR_H
