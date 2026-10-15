#include "IsochroneGenerator.h"
#include "RouteCostPolicy.h"
#include "../spatial/GeodesicPolygon.h"
#include <queue>
#include <algorithm>

namespace georescux::routing {

std::vector<IsochroneRing> IsochroneGenerator::computeIsochrones(
    const RouteGraph& graph,
    const std::string& originNodeId,
    const std::vector<int>& timeCutoffsMinutes
) {
    std::vector<IsochroneRing> rings;
    if (timeCutoffsMinutes.empty() || !graph.node(originNodeId)) {
        return rings;
    }

    std::vector<int> sortedCutoffs = timeCutoffsMinutes;
    std::sort(sortedCutoffs.begin(), sortedCutoffs.end());
    const double maxSeconds = sortedCutoffs.back() * 60.0;

    struct IsoSearchNode {
        std::string nodeId;
        double costSeconds;

        bool operator>(const IsoSearchNode& other) const {
            return costSeconds > other.costSeconds;
        }
    };

    std::priority_queue<IsoSearchNode, std::vector<IsoSearchNode>, std::greater<IsoSearchNode>> pq;
    std::unordered_map<std::string, double> travelTimeSec;

    travelTimeSec[originNodeId] = 0.0;
    pq.push({originNodeId, 0.0});

    while (!pq.empty()) {
        auto [u, timeU] = pq.top();
        pq.pop();

        if (timeU > travelTimeSec[u]) continue;
        if (timeU > maxSeconds) continue;

        for (const auto& [edge, neighbor] : graph.neighbors(u)) {
            auto edgeCostOpt = RouteCostPolicy::edgeCostMeters(edge, graph.hazards);
            if (!edgeCostOpt) continue;

            const double edgeSec = *edgeCostOpt / WALKING_SPEED_MPS;
            const double newTime = timeU + edgeSec;

            if (newTime <= maxSeconds) {
                auto it = travelTimeSec.find(neighbor.id);
                if (it == travelTimeSec.end() || newTime < it->second) {
                    travelTimeSec[neighbor.id] = newTime;
                    pq.push({neighbor.id, newTime});
                }
            }
        }
    }

    // Partition nodes into respective cutoff intervals
    for (int cutoffMin : sortedCutoffs) {
        const double cutoffSec = cutoffMin * 60.0;
        IsochroneRing ring{};
        ring.durationMinutes = cutoffMin;
        ring.maxDistanceMeters = cutoffSec * WALKING_SPEED_MPS;

        std::vector<spatial::Coordinate2D> coords;
        for (const auto& [nodeId, time] : travelTimeSec) {
            if (time <= cutoffSec) {
                ring.reachableNodeIds.push_back(nodeId);
                const auto* n = graph.node(nodeId);
                if (n) {
                    coords.push_back(spatial::Coordinate2D{n->latitude, n->longitude});
                }
            }
        }

        // Compute convex hull boundary of the reachability frontier
        if (!coords.empty()) {
            ring.boundaryPolygon = spatial::GeodesicPolygon::computeConvexHull(coords);
        }

        rings.push_back(std::move(ring));
    }

    return rings;
}

} // namespace georescux::routing
