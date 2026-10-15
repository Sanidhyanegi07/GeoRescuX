#ifndef GEORESCUX_TOOLS_GRAPH_EXTRACTOR_H
#define GEORESCUX_TOOLS_GRAPH_EXTRACTOR_H

#include "OsmPbfReader.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

namespace georescux::tools {

struct ExtractedNode {
    std::string id;
    double latitude;
    double longitude;
    bool isSafeHaven{false};
    std::string placeName;
};

struct ExtractedEdge {
    std::string fromNodeId;
    std::string toNodeId;
    double distanceMeters;
    std::string highwayType;
    bool blocked{false};
};

struct RegionBounds {
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;
};

/**
 * Graph extractor engine:
 * Filters OSM highway networks within regional bounding boxes (e.g. Uttarakhand, Himachal Pradesh),
 * tags hospitals/fire stations as safe havens, and writes out the routing graph JSON.
 */
class GraphExtractor {
public:
    explicit GraphExtractor(RegionBounds bounds);

    void addNode(const OsmNode& node);
    void addWay(const OsmWay& way);

    /**
     * Builds and serializes the complete regional graph to an output JSON file.
     */
    bool exportGraphJson(const std::string& outputPath) const;

    [[nodiscard]] size_t getNodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] size_t getEdgeCount() const noexcept { return edges_.size(); }
    [[nodiscard]] size_t getSafeHavenCount() const noexcept { return safeHavenCount_; }

private:
    RegionBounds bounds_;
    std::unordered_map<int64_t, OsmNode> rawNodes_;
    std::vector<ExtractedNode> nodes_;
    std::vector<ExtractedEdge> edges_;
    std::set<std::string> validHighwayTypes_;
    size_t safeHavenCount_{0};

    static double haversineDistance(double lat1, double lon1, double lat2, double lon2) noexcept;
};

} // namespace georescux::tools

#endif // GEORESCUX_TOOLS_GRAPH_EXTRACTOR_H
