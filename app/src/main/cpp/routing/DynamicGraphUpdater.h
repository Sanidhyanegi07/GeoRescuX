#ifndef GEORESCUX_ROUTING_DYNAMIC_GRAPH_UPDATER_H
#define GEORESCUX_ROUTING_DYNAMIC_GRAPH_UPDATER_H

#include "RouteGraph.h"
#include "../spatial/HazardPolygon.h"
#include <vector>
#include <string>
#include <mutex>

namespace georescux::routing {

/**
 * Thread-safe dynamic graph updater.
 * Applies active disaster reports (landslides, flash floods, collapsed bridges)
 * directly into the routing graph in real-time without expensive full-graph re-parsing.
 */
class DynamicGraphUpdater {
public:
    explicit DynamicGraphUpdater(RouteGraph& graph);

    /**
     * Dynamically blocks a road segment between fromNodeId and toNodeId.
     * If bidirectional is true, both directions are blocked.
     */
    bool blockEdge(const std::string& fromNodeId, const std::string& toNodeId, bool bidirectional = true);

    /**
     * Unblocks a previously blocked road segment.
     */
    bool unblockEdge(const std::string& fromNodeId, const std::string& toNodeId, bool bidirectional = true);

    /**
     * Adds or updates a penalty hazard on a road segment.
     */
    void addHazard(RoadHazard hazard);

    /**
     * Removes a hazard by its unique hazard ID.
     */
    bool removeHazard(const std::string& hazardId);

    /**
     * Automatically applies an active polygon hazard perimeter:
     * finds all graph edges intersecting the hazard polygon and blocks or penalizes them.
     * Returns count of affected edges.
     */
    size_t applyHazardPolygon(const spatial::HazardPolygon& hazardPoly, bool blockCompletely = true);

private:
    RouteGraph& graph_;
    std::mutex mutex_;
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_DYNAMIC_GRAPH_UPDATER_H
