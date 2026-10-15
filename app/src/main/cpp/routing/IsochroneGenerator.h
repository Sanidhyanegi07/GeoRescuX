#ifndef GEORESCUX_ROUTING_ISOCHRONE_GENERATOR_H
#define GEORESCUX_ROUTING_ISOCHRONE_GENERATOR_H

#include "RouteGraph.h"
#include "../spatial/HazardPolygon.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace georescux::routing {

struct IsochroneRing {
    int durationMinutes;
    double maxDistanceMeters;
    std::vector<std::string> reachableNodeIds;
    std::vector<spatial::Coordinate2D> boundaryPolygon;
};

/**
 * Evacuation Isochrone Wavefront Generator.
 * Calculates reachability frontiers from safe havens or disaster zones
 * across given time budgets (e.g. 15m, 30m, 45m, 60m evacuation envelopes).
 */
class IsochroneGenerator {
public:
    static constexpr double WALKING_SPEED_MPS = 1.38; // ~5.0 km/h

    /**
     * Computes isochrone rings outward from originNodeId for the specified time cutoffs.
     */
    static std::vector<IsochroneRing> computeIsochrones(
        const RouteGraph& graph,
        const std::string& originNodeId,
        const std::vector<int>& timeCutoffsMinutes
    );
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_ISOCHRONE_GENERATOR_H
