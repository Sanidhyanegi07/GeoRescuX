#ifndef GEORESCUX_ROUTING_ALTERNATIVE_ROUTE_FINDER_H
#define GEORESCUX_ROUTING_ALTERNATIVE_ROUTE_FINDER_H

#include "RouteGraph.h"
#include "EvacuationRoute.h"
#include <vector>
#include <string>

namespace georescux::routing {

struct MultiRouteOption {
    EvacuationRoute route;
    std::string routeLabel; // e.g. "Primary Safe Corridor", "Alternate Riverbank Route", "High-Ridge Route"
    double overlapPercentageWithPrimary;
    bool crossesHazardRisk;
};

/**
 * Multi-route generator for disaster resilience.
 * Generates K diverse, low-overlap evacuation paths so evacuees have
 * viable Plan B and Plan C fallback routes if primary roads become compromised.
 */
class AlternativeRouteFinder {
public:
    static constexpr double MAX_ACCEPTABLE_OVERLAP = 0.40; // 40% max edge overlap

    /**
     * Finds up to K distinct alternative evacuation routes between startId and destId.
     */
    static std::vector<MultiRouteOption> findKAlternativeRoutes(
        const RouteGraph& graph,
        const std::string& startId,
        const std::string& destId,
        size_t k = 3
    );

private:
    static double calculateEdgeOverlap(
        const std::vector<std::string>& pathA,
        const std::vector<std::string>& pathB
    ) noexcept;
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_ALTERNATIVE_ROUTE_FINDER_H
