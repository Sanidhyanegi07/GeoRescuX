#pragma once

#include <optional>
#include <string>
#include "RouteGraph.h"
#include "EvacuationRoute.h"

namespace georescux::routing {

class AStarPathfinder {
public:
    static std::optional<EvacuationRoute> findRoute(
        const RouteGraph& graph,
        const std::string& startId,
        const std::string& destinationId
    );

    static double haversineMeters(const RouteNode& from, const RouteNode& to);
};

} // namespace georescux::routing
