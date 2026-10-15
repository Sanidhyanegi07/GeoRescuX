#ifndef GEORESCUX_TOOLS_OSM_PBF_READER_H
#define GEORESCUX_TOOLS_OSM_PBF_READER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <memory>

namespace georescux::tools {

struct OsmNode {
    int64_t id;
    double latitude;
    double longitude;
    std::unordered_map<std::string, std::string> tags;
};

struct OsmWay {
    int64_t id;
    std::vector<int64_t> nodeIds;
    std::unordered_map<std::string, std::string> tags;
};

struct OsmRelationMember {
    int64_t memberId;
    std::string role;
    std::string type; // "node", "way", "relation"
};

struct OsmRelation {
    int64_t id;
    std::vector<OsmRelationMember> members;
    std::unordered_map<std::string, std::string> tags;
};

/**
 * Production C++20 OpenStreetMap PBF (Protocolbuffer Binary Format) streaming parser.
 * Implements high-throughput Protobuf wire format decoding, varint unpackers,
 * string table index resolution, and delta-encoded DenseNodes/Ways.
 */
class OsmPbfReader {
public:
    using NodeCallback = std::function<void(const OsmNode&)>;
    using WayCallback = std::function<void(const OsmWay&)>;
    using RelationCallback = std::function<void(const OsmRelation&)>;

    explicit OsmPbfReader(std::string filePath);

    /**
     * Parses the PBF file and invokes callbacks as entities are unpacked.
     */
    bool parse(NodeCallback onNode, WayCallback onWay, RelationCallback onRelation = nullptr);

    [[nodiscard]] uint64_t getTotalNodesParsed() const noexcept { return totalNodes_; }
    [[nodiscard]] uint64_t getTotalWaysParsed() const noexcept { return totalWays_; }
    [[nodiscard]] uint64_t getTotalRelationsParsed() const noexcept { return totalRelations_; }

    static uint64_t decodeVarint(const uint8_t*& ptr, const uint8_t* end) noexcept;
    static int64_t decodeZigZag(uint64_t n) noexcept;

private:
    std::string filePath_;
    uint64_t totalNodes_{0};
    uint64_t totalWays_{0};
    uint64_t totalRelations_{0};

    bool parsePrimitiveBlock(
        const uint8_t* data, size_t size,
        const NodeCallback& onNode,
        const WayCallback& onWay,
        const RelationCallback& onRelation
    );

    void parseDenseNodes(
        const uint8_t* ptr, const uint8_t* end,
        const std::vector<std::string>& stringTable,
        int64_t granularity, int64_t latOffset, int64_t lonOffset,
        const NodeCallback& onNode
    );
};

} // namespace georescux::tools

#endif // GEORESCUX_TOOLS_OSM_PBF_READER_H
