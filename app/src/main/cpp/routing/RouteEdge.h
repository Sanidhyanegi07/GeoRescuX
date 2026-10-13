#pragma once

#include <string>

namespace georescux::routing {

/**
 * An undirected road between two nodes. Blocked roads are excluded from
 * routing entirely and override every other cost consideration.
 */
struct RouteEdge {
    std::string fromNodeId;
    std::string toNodeId;
    double distanceMeters{0.0};
    bool blocked{false};

    RouteEdge() = default;
    RouteEdge(std::string from, std::string to, double meters, bool isBlocked = false)
        : fromNodeId(std::move(from)), toNodeId(std::move(to)), distanceMeters(meters), blocked(isBlocked) {}

    bool connects(const std::string& nodeA, const std::string& nodeB) const {
        return (fromNodeId == nodeA && toNodeId == nodeB) || (fromNodeId == nodeB && toNodeId == nodeA);
    }
};

} // namespace georescux::routing
