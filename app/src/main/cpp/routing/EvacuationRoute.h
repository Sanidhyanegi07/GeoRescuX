#pragma once

#include <string>
#include <vector>
#include "RoadHazard.h"

namespace georescux::routing {

/**
 * The result of a route search: ordered node IDs from start to destination,
 * total road distance, hazard-aware total cost, hazards on used edges,
 * and estimated duration in seconds (~5 m/s conservative disaster pace).
 */
struct EvacuationRoute {
    std::vector<std::string> nodeIds;
    double totalDistanceMeters{0.0};
    double totalCostMeters{0.0};
    std::vector<RoadHazard> hazardWarnings;
    long long estimatedDurationSeconds{0};

    bool empty() const {
        return nodeIds.empty();
    }
};

} // namespace georescux::routing
