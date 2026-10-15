#include "GraphExtractor.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace georescux::tools {

namespace {
constexpr double EARTH_RADIUS = 6371008.8;
constexpr double PI = 3.14159265358979323846;

inline double toRad(double d) { return d * (PI / 180.0); }
} // namespace

GraphExtractor::GraphExtractor(RegionBounds bounds)
    : bounds_(bounds) {
    validHighwayTypes_ = {
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "unclassified", "residential", "service", "living_street", "track"
    };
}

double GraphExtractor::haversineDistance(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = toRad(lat2 - lat1);
    double dLon = toRad(lon2 - lon1);
    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(toRad(lat1)) * std::cos(toRad(lat2)) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return EARTH_RADIUS * c;
}

void GraphExtractor::addNode(const OsmNode& node) {
    // Check bounding box
    if (node.latitude < bounds_.minLat || node.latitude > bounds_.maxLat ||
        node.longitude < bounds_.minLon || node.longitude > bounds_.maxLon) {
        return;
    }

    rawNodes_[node.id] = node;

    // Check if node is a safe haven (hospital, clinic, fire_station, police)
    bool safeHaven = false;
    std::string name;

    auto amIt = node.tags.find("amenity");
    if (amIt != node.tags.end()) {
        const auto& val = amIt->second;
        if (val == "hospital" || val == "clinic" || val == "police" || val == "fire_station" || val == "shelter") {
            safeHaven = true;
            safeHavenCount_++;
        }
    }

    auto nameIt = node.tags.find("name");
    if (nameIt != node.tags.end()) {
        name = nameIt->second;
    }

    nodes_.push_back(ExtractedNode{
        .id = std::to_string(node.id),
        .latitude = node.latitude,
        .longitude = node.longitude,
        .isSafeHaven = safeHaven,
        .placeName = name
    });
}

void GraphExtractor::addWay(const OsmWay& way) {
    auto hwIt = way.tags.find("highway");
    if (hwIt == way.tags.end() || validHighwayTypes_.find(hwIt->second) == validHighwayTypes_.end()) {
        return;
    }

    const std::string& hwType = hwIt->second;

    for (size_t i = 0; i + 1 < way.nodeIds.size(); ++i) {
        int64_t fromId = way.nodeIds[i];
        int64_t toId = way.nodeIds[i + 1];

        auto fromIt = rawNodes_.find(fromId);
        auto toIt = rawNodes_.find(toId);

        if (fromIt != rawNodes_.end() && toIt != rawNodes_.end()) {
            double dist = haversineDistance(
                fromIt->second.latitude, fromIt->second.longitude,
                toIt->second.latitude, toIt->second.longitude
            );

            edges_.push_back(ExtractedEdge{
                .fromNodeId = std::to_string(fromId),
                .toNodeId = std::to_string(toId),
                .distanceMeters = dist,
                .highwayType = hwType,
                .blocked = false
            });
        }
    }
}

bool GraphExtractor::exportGraphJson(const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out.is_open()) return false;

    out << "{\n  \"region\": {\n    \"version\": 2\n  },\n";

    // Nodes
    out << "  \"nodes\": [\n";
    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& n = nodes_[i];
        out << "    {\"id\":\"" << n.id << "\",\"latitude\":" << n.latitude
            << ",\"longitude\":" << n.longitude
            << ",\"isSafeHaven\":" << (n.isSafeHaven ? "true" : "false") << "}";
        if (i + 1 < nodes_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // Edges
    out << "  \"edges\": [\n";
    for (size_t i = 0; i < edges_.size(); ++i) {
        const auto& e = edges_[i];
        out << "    {\"fromNodeId\":\"" << e.fromNodeId
            << "\",\"toNodeId\":\"" << e.toNodeId
            << "\",\"distanceMeters\":" << e.distanceMeters
            << ",\"blocked\":false}";
        if (i + 1 < edges_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"hazards\": []\n}\n";

    return true;
}

} // namespace georescux::tools
