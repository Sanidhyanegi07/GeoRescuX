#include "AlternativeRouteFinder.h"
#include "AStarPathfinder.h"
#include <unordered_set>
#include <algorithm>

namespace georescux::routing {

double AlternativeRouteFinder::calculateEdgeOverlap(
    const std::vector<std::string>& pathA,
    const std::vector<std::string>& pathB
) noexcept {
    if (pathA.size() < 2 || pathB.size() < 2) return 0.0;

    std::unordered_set<std::string> edgesA;
    for (size_t i = 0; i + 1 < pathA.size(); ++i) {
        std::string e = (pathA[i] < pathA[i + 1]) ? (pathA[i] + "_" + pathA[i + 1]) : (pathA[i + 1] + "_" + pathA[i]);
        edgesA.insert(std::move(e));
    }

    size_t shared = 0;
    for (size_t i = 0; i + 1 < pathB.size(); ++i) {
        std::string e = (pathB[i] < pathB[i + 1]) ? (pathB[i] + "_" + pathB[i + 1]) : (pathB[i + 1] + "_" + pathB[i]);
        if (edgesA.count(e)) {
            shared++;
        }
    }

    return static_cast<double>(shared) / static_cast<double>(edgesA.size());
}

std::vector<MultiRouteOption> AlternativeRouteFinder::findKAlternativeRoutes(
    const RouteGraph& graph,
    const std::string& startId,
    const std::string& destId,
    size_t k
) {
    std::vector<MultiRouteOption> results;
    if (k == 0) return results;

    // 1. Primary Route
    auto primaryOpt = AStarPathfinder::findRoute(graph, startId, destId);
    if (!primaryOpt) return results;

    results.push_back(MultiRouteOption{
        .route = *primaryOpt,
        .routeLabel = "Primary Optimal Evacuation Path",
        .overlapPercentageWithPrimary = 0.0,
        .crossesHazardRisk = !primaryOpt->hazardWarnings.empty()
    });

    if (k == 1) return results;

    // 2. Penalization iteration to generate alternative disjoint corridors
    RouteGraph modifiedGraph = graph;

    for (size_t altIdx = 1; altIdx < k; ++altIdx) {
        const auto& lastPath = results.back().route.nodeIds;
        if (lastPath.size() < 2) break;

        // Apply heavy penalties on edges used in previous paths
        for (size_t i = 0; i + 1 < lastPath.size(); ++i) {
            std::string hId = "alt_pen_" + std::to_string(altIdx) + "_" + lastPath[i] + "_" + lastPath[i + 1];
            modifiedGraph.hazards.push_back(RoadHazard(hId, lastPath[i], lastPath[i + 1], 8000.0 * altIdx));
            modifiedGraph.hazards.push_back(RoadHazard(hId + "_rev", lastPath[i + 1], lastPath[i], 8000.0 * altIdx));
        }

        auto altOpt = AStarPathfinder::findRoute(modifiedGraph, startId, destId);
        if (!altOpt) break;

        double overlap = calculateEdgeOverlap(results[0].route.nodeIds, altOpt->nodeIds);
        if (overlap < 0.95) { // distinct enough
            std::string label = (altIdx == 1) ? "Secondary Fallback Corridor (Alternate B)"
                                              : "High-Ground Escape Route (Alternate C)";

            results.push_back(MultiRouteOption{
                .route = *altOpt,
                .routeLabel = label,
                .overlapPercentageWithPrimary = overlap * 100.0,
                .crossesHazardRisk = !altOpt->hazardWarnings.empty()
            });
        }
    }

    return results;
}

} // namespace georescux::routing
