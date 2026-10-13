#pragma once

#include <string>
#include <optional>
#include "RouteGraph.h"
#include "EvacuationRoute.h"

namespace georescux::routing {

class GraphJsonParser {
public:
    /**
     * Parses a RouteGraph from the JSON schema used by RegionGraphLoader:
     * {
     *   "region": { "id": "...", "version": 1 },
     *   "nodes": [ { "id": "...", "latitude": 28.6, "longitude": 77.2, "isSafeHaven": true }, ... ],
     *   "edges": [ { "fromNodeId": "...", "toNodeId": "...", "distanceMeters": 100.0, "blocked": false }, ... ],
     *   "hazards": [ { "id": "...", "fromNodeId": "...", "toNodeId": "...", "penaltyMeters": 50.0 }, ... ]
     * }
     */
    static std::optional<RouteGraph> parse(const std::string& json);

    /**
     * Serializes an EvacuationRoute to JSON:
     * {
     *   "found": true,
     *   "nodeIds": ["A", "B", "C"],
     *   "totalDistanceMeters": 120.0,
     *   "totalCostMeters": 120.0,
     *   "estimatedDurationSeconds": 60,
     *   "hazardWarnings": [{"id": "h1", "penaltyMeters": 10.0}]
     * }
     */
    static std::string serializeRoute(const std::optional<EvacuationRoute>& route);
};

} // namespace georescux::routing
