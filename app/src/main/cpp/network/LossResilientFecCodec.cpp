#include "LossResilientFecCodec.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

namespace georescux::network {

uint8_t LossResilientFecCodec::expTable_[512];
uint8_t LossResilientFecCodec::logTable_[256];
bool LossResilientFecCodec::tablesInitialized_ = false;

void LossResilientFecCodec::initTables() noexcept {
    if (tablesInitialized_) return;

    // Primitive polynomial for GF(2^8): x^8 + x^4 + x^3 + x^2 + 1 = 0x11D
    constexpr uint32_t PRIMITIVE_POLY = 0x11D;
    uint32_t val = 1;

    for (int i = 0; i < 255; ++i) {
        expTable_[i] = static_cast<uint8_t>(val);
        expTable_[i + 255] = static_cast<uint8_t>(val);
        logTable_[val] = static_cast<uint8_t>(i);

        val <<= 1;
        if (val & 0x100) {
            val ^= PRIMITIVE_POLY;
        }
    }
    logTable_[0] = 0; // log(0) undefined, mapped to 0
    tablesInitialized_ = true;
}

uint8_t LossResilientFecCodec::gfMul(uint8_t a, uint8_t b) noexcept {
    if (!tablesInitialized_) initTables();
    if (a == 0 || b == 0) return 0;
    return expTable_[logTable_[a] + logTable_[b]];
}

uint8_t LossResilientFecCodec::gfDiv(uint8_t a, uint8_t b) noexcept {
    if (!tablesInitialized_) initTables();
    if (a == 0) return 0;
    if (b == 0) return 0; // division by zero guarded
    return expTable_[logTable_[a] + 255 - logTable_[b]];
}

uint8_t LossResilientFecCodec::gfInv(uint8_t a) noexcept {
    if (!tablesInitialized_) initTables();
    if (a == 0) return 0;
    return expTable_[255 - logTable_[a]];
}

LossResilientFecCodec::LossResilientFecCodec(uint8_t dataShards, uint8_t parityShards)
    : k_(dataShards), m_(parityShards), n_(dataShards + parityShards) {
    if (k_ == 0 || m_ == 0 || n_ > 255) {
        throw std::invalid_argument("Invalid FEC shard configuration (must have 1 <= k, m and k+m <= 255)");
    }
    initTables();
    buildCauchyMatrix();
}

void LossResilientFecCodec::buildCauchyMatrix() {
    // Cauchy matrix dimension: m_ rows x k_ cols
    cauchyMatrix_.resize(m_ * k_);

    // Disjoint sets X and Y
    for (size_t i = 0; i < m_; ++i) {
        uint8_t x_i = static_cast<uint8_t>(i);
        for (size_t j = 0; j < k_; ++j) {
            uint8_t y_j = static_cast<uint8_t>(m_ + j);
            uint8_t diff = x_i ^ y_j; // GF(2^8) subtraction is XOR
            cauchyMatrix_[i * k_ + j] = gfInv(diff);
        }
    }
}

std::vector<FecBlock> LossResilientFecCodec::encode(
    uint16_t blockId,
    const uint8_t* data,
    size_t length
) const {
    if (data == nullptr || length == 0) return {};

    // First 4 bytes encode original payload length
    size_t totalPayloadLen = length + 4;
    size_t shardSize = (totalPayloadLen + k_ - 1) / k_;

    std::vector<std::vector<uint8_t>> dataShards(k_, std::vector<uint8_t>(shardSize, 0));

    // Pack 4-byte length header into shard 0
    dataShards[0][0] = static_cast<uint8_t>((length >> 24) & 0xFF);
    dataShards[0][1] = static_cast<uint8_t>((length >> 16) & 0xFF);
    dataShards[0][2] = static_cast<uint8_t>((length >> 8) & 0xFF);
    dataShards[0][3] = static_cast<uint8_t>(length & 0xFF);

    size_t byteIdx = 4;
    size_t currentShard = 0;
    for (size_t i = 0; i < length; ++i) {
        dataShards[currentShard][byteIdx] = data[i];
        byteIdx++;
        if (byteIdx == shardSize) {
            currentShard++;
            byteIdx = 0;
        }
    }

    // Generate parity shards via Cauchy generator matrix
    std::vector<std::vector<uint8_t>> parityShards(m_, std::vector<uint8_t>(shardSize, 0));
    for (size_t r = 0; r < m_; ++r) {
        for (size_t c = 0; c < k_; ++c) {
            uint8_t coeff = cauchyMatrix_[r * k_ + c];
            for (size_t b = 0; b < shardSize; ++b) {
                parityShards[r][b] ^= gfMul(coeff, dataShards[c][b]);
            }
        }
    }

    // Assemble FecBlock output
    std::vector<FecBlock> blocks;
    blocks.reserve(n_);

    for (size_t i = 0; i < k_; ++i) {
        blocks.push_back({blockId, n_, k_, static_cast<uint8_t>(i), std::move(dataShards[i])});
    }
    for (size_t i = 0; i < m_; ++i) {
        blocks.push_back({blockId, n_, k_, static_cast<uint8_t>(k_ + i), std::move(parityShards[i])});
    }

    return blocks;
}

bool LossResilientFecCodec::invertMatrix(std::vector<uint8_t>& mat, size_t dim) {
    std::vector<uint8_t> inv(dim * dim, 0);
    for (size_t i = 0; i < dim; ++i) {
        inv[i * dim + i] = 1;
    }

    // Gauss-Jordan elimination
    for (size_t col = 0; col < dim; ++col) {
        // Find pivot
        size_t pivotRow = col;
        while (pivotRow < dim && mat[pivotRow * dim + col] == 0) {
            pivotRow++;
        }
        if (pivotRow == dim) {
            return false; // singular matrix
        }

        // Swap rows
        if (pivotRow != col) {
            for (size_t c = 0; c < dim; ++c) {
                std::swap(mat[col * dim + c], mat[pivotRow * dim + c]);
                std::swap(inv[col * dim + c], inv[pivotRow * dim + c]);
            }
        }

        // Scale pivot row
        uint8_t pivotVal = mat[col * dim + col];
        uint8_t invPivot = gfInv(pivotVal);
        for (size_t c = 0; c < dim; ++c) {
            mat[col * dim + c] = gfMul(mat[col * dim + c], invPivot);
            inv[col * dim + c] = gfMul(inv[col * dim + c], invPivot);
        }

        // Eliminate other rows
        for (size_t r = 0; r < dim; ++r) {
            if (r != col) {
                uint8_t factor = mat[r * dim + col];
                if (factor != 0) {
                    for (size_t c = 0; c < dim; ++c) {
                        mat[r * dim + c] ^= gfMul(factor, mat[col * dim + c]);
                        inv[r * dim + c] ^= gfMul(factor, inv[col * dim + c]);
                    }
                }
            }
        }
    }

    mat = std::move(inv);
    return true;
}

bool LossResilientFecCodec::decode(
    const std::vector<FecBlock>& receivedShards,
    std::vector<uint8_t>& outRecoveredData
) const {
    if (receivedShards.size() < k_) {
        return false; // insufficient shards
    }

    // Select exactly k_ unique shards
    std::vector<const FecBlock*> selected;
    std::vector<bool> seen(n_, false);

    for (const auto& s : receivedShards) {
        if (s.shardIndex < n_ && !seen[s.shardIndex]) {
            seen[s.shardIndex] = true;
            selected.push_back(&s);
            if (selected.size() == k_) break;
        }
    }

    if (selected.size() < k_) return false;
    size_t shardSize = selected[0]->payload.size();

    // Check if we received all original data shards (no matrix inversion needed)
    bool hasAllData = true;
    for (size_t i = 0; i < k_; ++i) {
        if (!seen[i]) {
            hasAllData = false;
            break;
        }
    }

    std::vector<std::vector<uint8_t>> recoveredDataShards(k_, std::vector<uint8_t>(shardSize, 0));

    if (hasAllData) {
        for (const auto* s : selected) {
            if (s->shardIndex < k_) {
                recoveredDataShards[s->shardIndex] = s->payload;
            }
        }
    } else {
        // Construct K x K submatrix
        std::vector<uint8_t> submatrix(k_ * k_, 0);
        for (size_t r = 0; r < k_; ++r) {
            uint8_t sIdx = selected[r]->shardIndex;
            if (sIdx < k_) {
                // Identity row for original data shard
                submatrix[r * k_ + sIdx] = 1;
            } else {
                // Row from Cauchy matrix for parity shard
                uint8_t parityRow = sIdx - k_;
                for (size_t c = 0; c < k_; ++c) {
                    submatrix[r * k_ + c] = cauchyMatrix_[parityRow * k_ + c];
                }
            }
        }

        if (!invertMatrix(submatrix, k_)) {
            return false; // inversion failed
        }

        // Multiply inverted matrix by received shard vectors
        for (size_t r = 0; r < k_; ++r) {
            for (size_t c = 0; c < k_; ++c) {
                uint8_t coeff = submatrix[r * k_ + c];
                const auto& shardPayload = selected[c]->payload;
                for (size_t b = 0; b < shardSize; ++b) {
                    recoveredDataShards[r][b] ^= gfMul(coeff, shardPayload[b]);
                }
            }
        }
    }

    // Extract original length from header (first 4 bytes of shard 0)
    uint32_t originalLen = (static_cast<uint32_t>(recoveredDataShards[0][0]) << 24) |
                           (static_cast<uint32_t>(recoveredDataShards[0][1]) << 16) |
                           (static_cast<uint32_t>(recoveredDataShards[0][2]) << 8) |
                           static_cast<uint32_t>(recoveredDataShards[0][3]);

    if (originalLen > k_ * shardSize) {
        return false; // corrupted length
    }

    outRecoveredData.resize(originalLen);
    size_t byteIdx = 4;
    size_t currentShard = 0;

    for (size_t i = 0; i < originalLen; ++i) {
        outRecoveredData[i] = recoveredDataShards[currentShard][byteIdx];
        byteIdx++;
        if (byteIdx == shardSize) {
            currentShard++;
            byteIdx = 0;
        }
    }

    return true;
}

} // namespace georescux::network
