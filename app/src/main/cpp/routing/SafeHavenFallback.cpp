#include "SafeHavenFallback.h"
#include "AStarPathfinder.h"
#include <limits>

namespace georescux::routing {

std::optional<EvacuationRoute> SafeHavenFallback::findRoute(
    const RouteGraph& graph,
    const std::string& startId,
    const std::string& destinationId
) {
    // 1. Attempt direct destination route first
    auto directRoute = AStarPathfinder::findRoute(graph, startId, destinationId);
    if (directRoute) {
        return directRoute;
    }

    // 2. Safe-haven fallback: find reachable safe haven with minimum totalCostMeters
    std::optional<EvacuationRoute> bestHavenRoute;
    double lowestCost = std::numeric_limits<double>::infinity();

    for (const auto& node : graph.nodes) {
        if (node.isSafeHaven && node.id != startId) {
            auto havenRoute = AStarPathfinder::findRoute(graph, startId, node.id);
            if (havenRoute && havenRoute->totalCostMeters < lowestCost) {
                lowestCost = havenRoute->totalCostMeters;
                bestHavenRoute = havenRoute;
            }
        }
    }

    return bestHavenRoute;
}

} // namespace georescux::routing
