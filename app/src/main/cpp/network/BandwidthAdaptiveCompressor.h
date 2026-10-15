#ifndef GEORESCUX_BANDWIDTH_ADAPTIVE_COMPRESSOR_H
#define GEORESCUX_BANDWIDTH_ADAPTIVE_COMPRESSOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace georescux::network {

struct GpsTelemetryRecord {
    double latitude;
    double longitude;
    double altitudeMeters;
    uint32_t timestampSec;
    uint8_t batteryPercent;
    uint8_t triageColor; // 0=Black, 1=Red, 2=Yellow, 3=Green
};

/**
 * Bandwidth-Adaptive Telemetry Compressor for Ultra-Constrained Disaster Radios.
 * Combines Differential Geodesic Delta Encoding, LEB128 Variable-Length Bit Packing,
 * Run-Length Encoding (RLE), and Huffman Entropy Coding to compress telemetry packets
 * down to fit into single 20-byte BLE advertisements.
 */
class BandwidthAdaptiveCompressor {
public:
    BandwidthAdaptiveCompressor() = default;

    /**
     * Compresses a sequence of continuous GPS telemetry records using delta packing.
     */
    [[nodiscard]] static std::vector<uint8_t> compressTelemetryStream(
        const std::vector<GpsTelemetryRecord>& records
    );

    /**
     * Decompresses bytes back into original telemetry records.
     */
    [[nodiscard]] static std::vector<GpsTelemetryRecord> decompressTelemetryStream(
        const uint8_t* data,
        size_t length
    );

    /**
     * Run-Length Encodes a binary hazard grid mask.
     */
    [[nodiscard]] static std::vector<uint8_t> compressHazardMaskRle(
        const std::vector<uint8_t>& mask
    );

    /**
     * Decompresses an RLE-encoded binary hazard grid mask.
     */
    [[nodiscard]] static std::vector<uint8_t> decompressHazardMaskRle(
        const uint8_t* data,
        size_t length,
        size_t expectedSize
    );

    /**
     * Huffman Entropy Encoding for arbitrary text or telemetry messages.
     */
    [[nodiscard]] static std::vector<uint8_t> huffmanCompress(
        const uint8_t* input,
        size_t length
    );

    /**
     * Huffman Entropy Decoding.
     */
    [[nodiscard]] static std::vector<uint8_t> huffmanDecompress(
        const uint8_t* input,
        size_t length,
        size_t originalSize
    );

    // Bit-level stream serialization utilities
    class BitWriter {
    public:
        void writeBit(uint8_t bit);
        void writeBits(uint32_t val, uint8_t numBits);
        [[nodiscard]] std::vector<uint8_t> flush();
    private:
        std::vector<uint8_t> bytes_;
        uint8_t currentByte_ = 0;
        uint8_t bitCount_ = 0;
    };

    class BitReader {
    public:
        BitReader(const uint8_t* data, size_t length);
        uint8_t readBit();
        uint32_t readBits(uint8_t numBits);
        [[nodiscard]] bool hasRemaining() const noexcept;
    private:
        const uint8_t* data_;
        size_t length_;
        size_t byteIdx_ = 0;
        uint8_t bitIdx_ = 0;
    };
};

} // namespace georescux::network

#endif // GEORESCUX_BANDWIDTH_ADAPTIVE_COMPRESSOR_H
