#include "BandwidthAdaptiveCompressor.h"
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <stdexcept>

namespace georescux::network {

// BitWriter Implementation
void BandwidthAdaptiveCompressor::BitWriter::writeBit(uint8_t bit) {
    currentByte_ = (currentByte_ << 1) | (bit & 1);
    bitCount_++;
    if (bitCount_ == 8) {
        bytes_.push_back(currentByte_);
        currentByte_ = 0;
        bitCount_ = 0;
    }
}

void BandwidthAdaptiveCompressor::BitWriter::writeBits(uint32_t val, uint8_t numBits) {
    for (int i = numBits - 1; i >= 0; --i) {
        writeBit(static_cast<uint8_t>((val >> i) & 1));
    }
}

std::vector<uint8_t> BandwidthAdaptiveCompressor::BitWriter::flush() {
    if (bitCount_ > 0) {
        currentByte_ <<= (8 - bitCount_); // align remaining bits to left
        bytes_.push_back(currentByte_);
        currentByte_ = 0;
        bitCount_ = 0;
    }
    return std::move(bytes_);
}

// BitReader Implementation
BandwidthAdaptiveCompressor::BitReader::BitReader(const uint8_t* data, size_t length)
    : data_(data), length_(length) {}

bool BandwidthAdaptiveCompressor::BitReader::hasRemaining() const noexcept {
    return byteIdx_ < length_;
}

uint8_t BandwidthAdaptiveCompressor::BitReader::readBit() {
    if (!hasRemaining()) return 0;

    uint8_t bit = (data_[byteIdx_] >> (7 - bitIdx_)) & 1;
    bitIdx_++;
    if (bitIdx_ == 8) {
        bitIdx_ = 0;
        byteIdx_++;
    }
    return bit;
}

uint32_t BandwidthAdaptiveCompressor::BitReader::readBits(uint8_t numBits) {
    uint32_t val = 0;
    for (uint8_t i = 0; i < numBits; ++i) {
        val = (val << 1) | readBit();
    }
    return val;
}

namespace {
uint32_t encodeZigzag(int32_t n) noexcept {
    return static_cast<uint32_t>((n << 1) ^ (n >> 31));
}

int32_t decodeZigzag(uint32_t n) noexcept {
    return static_cast<int32_t>((n >> 1) ^ -(n & 1));
}

void writeVarint(std::vector<uint8_t>& buf, uint32_t val) {
    while (val >= 0x80) {
        buf.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(val & 0x7F));
}

uint32_t readVarint(const uint8_t*& ptr, const uint8_t* end) {
    uint32_t val = 0;
    uint32_t shift = 0;
    while (ptr < end) {
        uint8_t b = *ptr++;
        val |= static_cast<uint32_t>(b & 0x7F) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }
    return val;
}
} // namespace

std::vector<uint8_t> BandwidthAdaptiveCompressor::compressTelemetryStream(
    const std::vector<GpsTelemetryRecord>& records
) {
    if (records.empty()) return {};

    std::vector<uint8_t> buffer;
    writeVarint(buffer, static_cast<uint32_t>(records.size()));

    // Anchor record (scaled to microdegrees)
    int32_t prevLat = static_cast<int32_t>(std::round(records[0].latitude * 1e6));
    int32_t prevLon = static_cast<int32_t>(std::round(records[0].longitude * 1e6));
    int32_t prevAlt = static_cast<int32_t>(std::round(records[0].altitudeMeters));
    uint32_t prevTime = records[0].timestampSec;

    writeVarint(buffer, encodeZigzag(prevLat));
    writeVarint(buffer, encodeZigzag(prevLon));
    writeVarint(buffer, encodeZigzag(prevAlt));
    writeVarint(buffer, prevTime);
    buffer.push_back(records[0].batteryPercent);
    buffer.push_back(records[0].triageColor);

    // Delta encode remaining records
    for (size_t i = 1; i < records.size(); ++i) {
        int32_t curLat = static_cast<int32_t>(std::round(records[i].latitude * 1e6));
        int32_t curLon = static_cast<int32_t>(std::round(records[i].longitude * 1e6));
        int32_t curAlt = static_cast<int32_t>(std::round(records[i].altitudeMeters));
        uint32_t curTime = records[i].timestampSec;

        writeVarint(buffer, encodeZigzag(curLat - prevLat));
        writeVarint(buffer, encodeZigzag(curLon - prevLon));
        writeVarint(buffer, encodeZigzag(curAlt - prevAlt));
        writeVarint(buffer, curTime - prevTime);
        buffer.push_back(records[i].batteryPercent);
        buffer.push_back(records[i].triageColor);

        prevLat = curLat;
        prevLon = curLon;
        prevAlt = curAlt;
        prevTime = curTime;
    }

    return buffer;
}

std::vector<GpsTelemetryRecord> BandwidthAdaptiveCompressor::decompressTelemetryStream(
    const uint8_t* data,
    size_t length
) {
    if (data == nullptr || length == 0) return {};

    const uint8_t* ptr = data;
    const uint8_t* end = data + length;

    uint32_t count = readVarint(ptr, end);
    std::vector<GpsTelemetryRecord> records;
    records.reserve(count);

    if (ptr >= end) return records;

    // Anchor record
    int32_t curLat = decodeZigzag(readVarint(ptr, end));
    int32_t curLon = decodeZigzag(readVarint(ptr, end));
    int32_t curAlt = decodeZigzag(readVarint(ptr, end));
    uint32_t curTime = readVarint(ptr, end);
    uint8_t battery = (ptr < end) ? *ptr++ : 100;
    uint8_t triage = (ptr < end) ? *ptr++ : 3;

    records.push_back({
        curLat / 1e6,
        curLon / 1e6,
        static_cast<double>(curAlt),
        curTime,
        battery,
        triage
    });

    for (size_t i = 1; i < count && ptr < end; ++i) {
        curLat += decodeZigzag(readVarint(ptr, end));
        curLon += decodeZigzag(readVarint(ptr, end));
        curAlt += decodeZigzag(readVarint(ptr, end));
        curTime += readVarint(ptr, end);
        battery = (ptr < end) ? *ptr++ : 100;
        triage = (ptr < end) ? *ptr++ : 3;

        records.push_back({
            curLat / 1e6,
            curLon / 1e6,
            static_cast<double>(curAlt),
            curTime,
            battery,
            triage
        });
    }

    return records;
}

std::vector<uint8_t> BandwidthAdaptiveCompressor::compressHazardMaskRle(
    const std::vector<uint8_t>& mask
) {
    if (mask.empty()) return {};

    std::vector<uint8_t> compressed;
    uint8_t currentVal = mask[0];
    uint8_t runLength = 1;

    for (size_t i = 1; i < mask.size(); ++i) {
        if (mask[i] == currentVal && runLength < 255) {
            runLength++;
        } else {
            compressed.push_back(currentVal);
            compressed.push_back(runLength);
            currentVal = mask[i];
            runLength = 1;
        }
    }
    compressed.push_back(currentVal);
    compressed.push_back(runLength);

    return compressed;
}

std::vector<uint8_t> BandwidthAdaptiveCompressor::decompressHazardMaskRle(
    const uint8_t* data,
    size_t length,
    size_t expectedSize
) {
    if (data == nullptr || length < 2) return {};

    std::vector<uint8_t> result;
    result.reserve(expectedSize);

    for (size_t i = 0; i + 1 < length; i += 2) {
        uint8_t val = data[i];
        uint8_t count = data[i + 1];
        for (uint8_t k = 0; k < count; ++k) {
            result.push_back(val);
        }
    }

    return result;
}

// Huffman Tree Nodes
struct HuffmanNode {
    uint8_t symbol;
    uint32_t frequency;
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;
};

struct NodeCompare {
    bool operator()(const std::shared_ptr<HuffmanNode>& a, const std::shared_ptr<HuffmanNode>& b) const {
        return a->frequency > b->frequency;
    }
};

void buildHuffmanCodes(
    const std::shared_ptr<HuffmanNode>& node,
    uint32_t currentCode,
    uint8_t codeLen,
    std::unordered_map<uint8_t, std::pair<uint32_t, uint8_t>>& codeTable
) {
    if (!node) return;
    if (!node->left && !node->right) {
        codeTable[node->symbol] = {currentCode, std::max(uint8_t{1}, codeLen)};
        return;
    }
    buildHuffmanCodes(node->left, (currentCode << 1) | 0, codeLen + 1, codeTable);
    buildHuffmanCodes(node->right, (currentCode << 1) | 1, codeLen + 1, codeTable);
}

std::vector<uint8_t> BandwidthAdaptiveCompressor::huffmanCompress(
    const uint8_t* input,
    size_t length
) {
    if (input == nullptr || length == 0) return {};

    // 1. Histogram
    std::unordered_map<uint8_t, uint32_t> freq;
    for (size_t i = 0; i < length; ++i) {
        freq[input[i]]++;
    }

    // 2. Priority queue
    std::priority_queue<std::shared_ptr<HuffmanNode>, std::vector<std::shared_ptr<HuffmanNode>>, NodeCompare> pq;
    for (const auto& [sym, count] : freq) {
        auto n = std::make_shared<HuffmanNode>();
        n->symbol = sym;
        n->frequency = count;
        pq.push(n);
    }

    if (pq.size() == 1) {
        auto dummy = std::make_shared<HuffmanNode>();
        dummy->symbol = 0;
        dummy->frequency = 0;
        pq.push(dummy);
    }

    // 3. Tree construction
    while (pq.size() > 1) {
        auto left = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();

        auto parent = std::make_shared<HuffmanNode>();
        parent->symbol = 0;
        parent->frequency = left->frequency + right->frequency;
        parent->left = left;
        parent->right = right;
        pq.push(parent);
    }

    auto root = pq.top();
    std::unordered_map<uint8_t, std::pair<uint32_t, uint8_t>> codeTable;
    buildHuffmanCodes(root, 0, 0, codeTable);

    // 4. Serialize header (frequency table)
    std::vector<uint8_t> header;
    header.push_back(static_cast<uint8_t>(freq.size()));
    for (const auto& [sym, count] : freq) {
        header.push_back(sym);
        writeVarint(header, count);
    }

    // 5. Bit-pack encoded payload
    BitWriter writer;
    for (size_t i = 0; i < length; ++i) {
        const auto& [code, len] = codeTable[input[i]];
        writer.writeBits(code, len);
    }

    auto payload = writer.flush();
    header.insert(header.end(), payload.begin(), payload.end());
    return header;
}

std::vector<uint8_t> BandwidthAdaptiveCompressor::huffmanDecompress(
    const uint8_t* input,
    size_t length,
    size_t originalSize
) {
    if (input == nullptr || length == 0 || originalSize == 0) return {};

    const uint8_t* ptr = input;
    const uint8_t* end = input + length;

    uint8_t numSymbols = *ptr++;
    size_t symbolCount = (numSymbols == 0) ? 256 : numSymbols;

    std::priority_queue<std::shared_ptr<HuffmanNode>, std::vector<std::shared_ptr<HuffmanNode>>, NodeCompare> pq;
    for (size_t i = 0; i < symbolCount && ptr < end; ++i) {
        uint8_t sym = *ptr++;
        uint32_t count = readVarint(ptr, end);
        auto n = std::make_shared<HuffmanNode>();
        n->symbol = sym;
        n->frequency = count;
        pq.push(n);
    }

    if (pq.empty()) return {};

    if (pq.size() == 1) {
        auto dummy = std::make_shared<HuffmanNode>();
        dummy->symbol = 0;
        dummy->frequency = 0;
        pq.push(dummy);
    }

    while (pq.size() > 1) {
        auto left = pq.top(); pq.pop();
        auto right = pq.top(); pq.pop();

        auto parent = std::make_shared<HuffmanNode>();
        parent->symbol = 0;
        parent->frequency = left->frequency + right->frequency;
        parent->left = left;
        parent->right = right;
        pq.push(parent);
    }

    auto root = pq.top();
    BitReader reader(ptr, end - ptr);
    std::vector<uint8_t> decompressed;
    decompressed.reserve(originalSize);

    while (decompressed.size() < originalSize && reader.hasRemaining()) {
        auto curr = root;
        while (curr->left || curr->right) {
            uint8_t b = reader.readBit();
            curr = (b == 0) ? curr->left : curr->right;
            if (!curr) break;
        }
        if (curr) {
            decompressed.push_back(curr->symbol);
        }
    }

    return decompressed;
}

} // namespace georescux::network
