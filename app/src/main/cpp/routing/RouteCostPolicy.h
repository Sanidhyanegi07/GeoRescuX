#pragma once

#include <vector>
#include <optional>
#include "RouteEdge.h"
#include "RoadHazard.h"

namespace georescux::routing {

class RouteCostPolicy {
public:
    static std::optional<double> edgeCostMeters(const RouteEdge& edge, const std::vector<RoadHazard>& hazards) {
        if (edge.blocked) {
            return std::nullopt;
        }
        double cost = edge.distanceMeters;
        for (const auto& hazard : hazards) {
            if (hazard.affects(edge.fromNodeId, edge.toNodeId)) {
                cost += hazard.penaltyMeters;
            }
        }
        return cost;
    }
};

} // namespace georescux::routing
