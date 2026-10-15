#ifndef GEORESCUX_LOSS_RESILIENT_FEC_CODEC_H
#define GEORESCUX_LOSS_RESILIENT_FEC_CODEC_H

#include <vector>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace georescux::network {

struct FecBlock {
    uint16_t blockId;
    uint8_t totalShards; // N (e.g. 10 shards)
    uint8_t dataShards;  // K (e.g. 6 data shards)
    uint8_t shardIndex;  // 0 to N-1
    std::vector<uint8_t> payload;
};

/**
 * Cauchy Reed-Solomon Erasure Coding and Forward Error Correction (FEC) Engine.
 * Operates over Galois Field GF(2^8) with irreducible polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x11D).
 * Guarantees recovery of all original data packets over lossy BLE mesh / disaster radio
 * as long as ANY K out of N shards are received, requiring zero retransmissions.
 */
class LossResilientFecCodec {
public:
    LossResilientFecCodec(uint8_t dataShards, uint8_t parityShards);

    /**
     * Encodes arbitrary binary data into N = (dataShards + parityShards) FEC blocks.
     */
    [[nodiscard]] std::vector<FecBlock> encode(
        uint16_t blockId,
        const uint8_t* data,
        size_t length
    ) const;

    /**
     * Decodes received shards into original payload.
     * Requires at least dataShards (K) unique shards from the same block.
     * Returns true if reconstruction succeeded.
     */
    bool decode(
        const std::vector<FecBlock>& receivedShards,
        std::vector<uint8_t>& outRecoveredData
    ) const;

    // Galois Field GF(2^8) Arithmetic Primitive Functions
    static uint8_t gfAdd(uint8_t a, uint8_t b) noexcept { return a ^ b; }
    static uint8_t gfSub(uint8_t a, uint8_t b) noexcept { return a ^ b; }
    static uint8_t gfMul(uint8_t a, uint8_t b) noexcept;
    static uint8_t gfDiv(uint8_t a, uint8_t b) noexcept;
    static uint8_t gfInv(uint8_t a) noexcept;

private:
    uint8_t k_; // data shards
    uint8_t m_; // parity shards
    uint8_t n_; // total shards = k + m

    static uint8_t expTable_[512];
    static uint8_t logTable_[256];
    static bool tablesInitialized_;
    static void initTables() noexcept;

    // Cauchy Generator Matrix: A[i][j] = 1 / (X_i ^ Y_j)
    std::vector<uint8_t> cauchyMatrix_;
    void buildCauchyMatrix();

    static bool invertMatrix(std::vector<uint8_t>& matrix, size_t dim);
};

} // namespace georescux::network

#endif // GEORESCUX_LOSS_RESILIENT_FEC_CODEC_H
