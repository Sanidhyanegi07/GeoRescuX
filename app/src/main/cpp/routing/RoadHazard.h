#pragma once

#include <string>

namespace georescux::routing {

/**
 * A hazard on the undirected road between two nodes. Adds a penalty (in meters)
 * to that edge's routing cost.
 */
struct RoadHazard {
    std::string id;
    std::string fromNodeId;
    std::string toNodeId;
    double penaltyMeters{0.0};

    RoadHazard() = default;
    RoadHazard(std::string id, std::string from, std::string to, double penalty)
        : id(std::move(id)), fromNodeId(std::move(from)), toNodeId(std::move(to)), penaltyMeters(penalty) {}

    bool affects(const std::string& nodeA, const std::string& nodeB) const {
        return (fromNodeId == nodeA && toNodeId == nodeB) || (fromNodeId == nodeB && toNodeId == nodeA);
    }
};

} // namespace georescux::routing
