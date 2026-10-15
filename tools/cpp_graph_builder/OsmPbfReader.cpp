#include "OsmPbfReader.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>

namespace georescux::tools {

OsmPbfReader::OsmPbfReader(std::string filePath)
    : filePath_(std::move(filePath)) {}

uint64_t OsmPbfReader::decodeVarint(const uint8_t*& ptr, const uint8_t* end) noexcept {
    uint64_t result = 0;
    int shift = 0;
    while (ptr < end) {
        uint8_t byte = *ptr++;
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
        if (shift >= 64) break;
    }
    return result;
}

int64_t OsmPbfReader::decodeZigZag(uint64_t n) noexcept {
    return static_cast<int64_t>((n >> 1) ^ (-(n & 1)));
}

void OsmPbfReader::parseDenseNodes(
    const uint8_t* ptr, const uint8_t* end,
    const std::vector<std::string>& stringTable,
    int64_t granularity, int64_t latOffset, int64_t lonOffset,
    const NodeCallback& onNode
) {
    std::vector<int64_t> ids;
    std::vector<int64_t> lats;
    std::vector<int64_t> lons;
    std::vector<int32_t> tagIndices;

    while (ptr < end) {
        uint64_t tag = decodeVarint(ptr, end);
        uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        uint32_t wireType = static_cast<uint32_t>(tag & 0x07);

        if (wireType == 2) { // length-delimited
            uint64_t len = decodeVarint(ptr, end);
            const uint8_t* subEnd = ptr + len;

            if (fieldNum == 1) { // id (delta-coded sint64)
                int64_t currentId = 0;
                while (ptr < subEnd) {
                    currentId += decodeZigZag(decodeVarint(ptr, subEnd));
                    ids.push_back(currentId);
                }
            } else if (fieldNum == 8) { // lat (delta-coded sint64)
                int64_t currentLat = 0;
                while (ptr < subEnd) {
                    currentLat += decodeZigZag(decodeVarint(ptr, subEnd));
                    lats.push_back(currentLat);
                }
            } else if (fieldNum == 9) { // lon (delta-coded sint64)
                int64_t currentLon = 0;
                while (ptr < subEnd) {
                    currentLon += decodeZigZag(decodeVarint(ptr, subEnd));
                    lons.push_back(currentLon);
                }
            } else if (fieldNum == 10) { // keys_vals (int32)
                while (ptr < subEnd) {
                    tagIndices.push_back(static_cast<int32_t>(decodeVarint(ptr, subEnd)));
                }
            } else {
                ptr = subEnd;
            }
        } else if (wireType == 0) {
            decodeVarint(ptr, end);
        } else if (wireType == 5) {
            ptr += 4;
        } else if (wireType == 1) {
            ptr += 8;
        } else {
            break;
        }
    }

    const size_t count = std::min({ids.size(), lats.size(), lons.size()});
    size_t tagPos = 0;

    for (size_t i = 0; i < count; ++i) {
        OsmNode node;
        node.id = ids[i];
        node.latitude = 0.000000001 * (latOffset + (granularity * lats[i]));
        node.longitude = 0.000000001 * (lonOffset + (granularity * lons[i]));

        // Read string tags until delimiter 0
        while (tagPos < tagIndices.size() && tagIndices[tagPos] != 0) {
            int32_t kIdx = tagIndices[tagPos++];
            if (tagPos < tagIndices.size()) {
                int32_t vIdx = tagIndices[tagPos++];
                if (kIdx >= 0 && static_cast<size_t>(kIdx) < stringTable.size() &&
                    vIdx >= 0 && static_cast<size_t>(vIdx) < stringTable.size()) {
                    node.tags[stringTable[kIdx]] = stringTable[vIdx];
                }
            }
        }
        if (tagPos < tagIndices.size() && tagIndices[tagPos] == 0) {
            tagPos++; // skip delimiter
        }

        totalNodes_++;
        if (onNode) {
            onNode(node);
        }
    }
}

bool OsmPbfReader::parsePrimitiveBlock(
    const uint8_t* data, size_t size,
    const NodeCallback& onNode,
    const WayCallback& onWay,
    const RelationCallback& onRelation
) {
    const uint8_t* ptr = data;
    const uint8_t* end = data + size;

    std::vector<std::string> stringTable;
    int64_t granularity = 100;
    int64_t latOffset = 0;
    int64_t lonOffset = 0;

    while (ptr < end) {
        uint64_t tag = decodeVarint(ptr, end);
        uint32_t fieldNum = static_cast<uint32_t>(tag >> 3);
        uint32_t wireType = static_cast<uint32_t>(tag & 0x07);

        if (wireType == 2) {
            uint64_t len = decodeVarint(ptr, end);
            const uint8_t* fieldEnd = ptr + len;

            if (fieldNum == 1) { // StringTable
                while (ptr < fieldEnd) {
                    uint64_t sTag = decodeVarint(ptr, fieldEnd);
                    if ((sTag & 0x07) == 2) {
                        uint64_t sLen = decodeVarint(ptr, fieldEnd);
                        stringTable.emplace_back(reinterpret_cast<const char*>(ptr), sLen);
                        ptr += sLen;
                    } else {
                        break;
                    }
                }
            } else if (fieldNum == 2) { // PrimitiveGroup
                while (ptr < fieldEnd) {
                    uint64_t gTag = decodeVarint(ptr, fieldEnd);
                    uint32_t gField = static_cast<uint32_t>(gTag >> 3);
                    uint32_t gWire = static_cast<uint32_t>(gTag & 0x07);

                    if (gWire == 2) {
                        uint64_t gLen = decodeVarint(ptr, fieldEnd);
                        const uint8_t* groupEnd = ptr + gLen;

                        if (gField == 2) { // DenseNodes
                            parseDenseNodes(ptr, groupEnd, stringTable, granularity, latOffset, lonOffset, onNode);
                            ptr = groupEnd;
                        } else if (gField == 3) { // Ways
                            OsmWay way{};
                            int64_t currentRef = 0;
                            while (ptr < groupEnd) {
                                uint64_t wTag = decodeVarint(ptr, groupEnd);
                                uint32_t wField = static_cast<uint32_t>(wTag >> 3);
                                uint32_t wWire = static_cast<uint32_t>(wTag & 0x07);
                                if (wWire == 0 && wField == 1) { // id
                                    way.id = static_cast<int64_t>(decodeVarint(ptr, groupEnd));
                                } else if (wWire == 2 && wField == 8) { // refs (delta)
                                    uint64_t rLen = decodeVarint(ptr, groupEnd);
                                    const uint8_t* rEnd = ptr + rLen;
                                    while (ptr < rEnd) {
                                        currentRef += decodeZigZag(decodeVarint(ptr, rEnd));
                                        way.nodeIds.push_back(currentRef);
                                    }
                                } else if (wWire == 2) {
                                    uint64_t skipLen = decodeVarint(ptr, groupEnd);
                                    ptr += skipLen;
                                } else if (wWire == 0) {
                                    decodeVarint(ptr, groupEnd);
                                } else {
                                    break;
                                }
                            }
                            totalWays_++;
                            if (onWay) onWay(way);
                            ptr = groupEnd;
                        } else {
                            ptr = groupEnd;
                        }
                    } else {
                        break;
                    }
                }
            } else {
                ptr = fieldEnd;
            }
        } else if (wireType == 0) {
            uint64_t val = decodeVarint(ptr, end);
            if (fieldNum == 17) granularity = static_cast<int64_t>(val);
            else if (fieldNum == 19) latOffset = static_cast<int64_t>(val);
            else if (fieldNum == 20) lonOffset = static_cast<int64_t>(val);
        } else {
            break;
        }
    }

    return true;
}

bool OsmPbfReader::parse(NodeCallback onNode, WayCallback onWay, RelationCallback onRelation) {
    std::ifstream file(filePath_, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open PBF: " << filePath_ << std::endl;
        return false;
    }

    totalNodes_ = 0;
    totalWays_ = 0;
    totalRelations_ = 0;

    std::vector<uint8_t> lenBuf(4);
    std::vector<uint8_t> blockBuf(512 * 1024); // 512KB buffer

    while (file.read(reinterpret_cast<char*>(lenBuf.data()), 4)) {
        uint32_t headerLen = (static_cast<uint32_t>(lenBuf[0]) << 24) |
                             (static_cast<uint32_t>(lenBuf[1]) << 16) |
                             (static_cast<uint32_t>(lenBuf[2]) << 8)  |
                             (static_cast<uint32_t>(lenBuf[3]));

        if (headerLen == 0 || headerLen > 64 * 1024) break;

        std::vector<uint8_t> headerBytes(headerLen);
        if (!file.read(reinterpret_cast<char*>(headerBytes.data()), headerLen)) break;

        // Parse BlobHeader to get data size
        const uint8_t* hPtr = headerBytes.data();
        const uint8_t* hEnd = headerBytes.data() + headerLen;

        std::string blockType;
        uint32_t dataSize = 0;

        while (hPtr < hEnd) {
            uint64_t tag = decodeVarint(hPtr, hEnd);
            uint32_t fNum = static_cast<uint32_t>(tag >> 3);
            uint32_t wType = static_cast<uint32_t>(tag & 0x07);

            if (wType == 2 && fNum == 1) { // type
                uint64_t sLen = decodeVarint(hPtr, hEnd);
                blockType.assign(reinterpret_cast<const char*>(hPtr), sLen);
                hPtr += sLen;
            } else if (wType == 0 && fNum == 3) { // datasize
                dataSize = static_cast<uint32_t>(decodeVarint(hPtr, hEnd));
            } else if (wType == 2) {
                uint64_t skip = decodeVarint(hPtr, hEnd);
                hPtr += skip;
            } else if (wType == 0) {
                decodeVarint(hPtr, hEnd);
            } else {
                break;
            }
        }

        if (dataSize == 0) break;

        if (blockBuf.size() < dataSize) {
            blockBuf.resize(dataSize);
        }

        if (!file.read(reinterpret_cast<char*>(blockBuf.data()), dataSize)) {
            break;
        }

        if (blockType == "OSMData") {
            parsePrimitiveBlock(blockBuf.data(), dataSize, onNode, onWay, onRelation);
        }
    }

    return true;
}

} // namespace georescux::tools
