#ifndef GEORESCUX_ROUTING_BIDIRECTIONAL_ASTAR_H
#define GEORESCUX_ROUTING_BIDIRECTIONAL_ASTAR_H

#include "RouteGraph.h"
#include "EvacuationRoute.h"
#include <optional>
#include <string>

namespace georescux::routing {

/**
 * High-performance Bidirectional A* pathfinder.
 * Runs two simultaneous searches (forward from origin and reverse from destination)
 * meeting at a common frontier to dramatically reduce search space on large regional graphs.
 */
class BidirectionalAStar {
public:
    /**
     * Finds the lowest-cost evacuation route between startNodeId and destinationNodeId.
     * Returns nullopt if no valid path exists.
     */
    static std::optional<EvacuationRoute> findRoute(
        const RouteGraph& graph,
        const std::string& startNodeId,
        const std::string& destinationNodeId
    );
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_BIDIRECTIONAL_ASTAR_H
