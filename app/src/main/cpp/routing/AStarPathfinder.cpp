#include "AStarPathfinder.h"
#include "RouteCostPolicy.h"
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace georescux::routing {

namespace {

constexpr double EARTH_RADIUS_METERS = 6371000.0;
constexpr double PI = 3.14159265358979323846;
constexpr double ASSUMED_SPEED_MPS = 5.0;
constexpr long long MIN_SECONDS = 60LL;

inline double toRadians(double deg) {
    return deg * (PI / 180.0);
}

struct SearchNode {
    std::string nodeId;
    double costSoFar{0.0};
    double estimatedTotalCost{0.0};

    bool operator>(const SearchNode& other) const {
        if (std::abs(estimatedTotalCost - other.estimatedTotalCost) > 1e-9) {
            return estimatedTotalCost > other.estimatedTotalCost;
        }
        return nodeId > other.nodeId;
    }
};

long long estimateSeconds(double distanceMeters) {
    if (distanceMeters <= 0.0) return 0LL;
    long long secs = static_cast<long long>(distanceMeters / ASSUMED_SPEED_MPS);
    return std::max(secs, MIN_SECONDS);
}

EvacuationRoute buildRoute(
    const RouteGraph& graph,
    const std::unordered_map<std::string, std::string>& cameFrom,
    const std::string& destinationId
) {
    std::vector<std::string> path;
    path.push_back(destinationId);
    std::string current = destinationId;

    while (true) {
        auto it = cameFrom.find(current);
        if (it == cameFrom.end()) break;
        current = it->second;
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());

    double totalDistance = 0.0;
    double totalCost = 0.0;
    std::vector<RoadHazard> hazardWarnings;
    std::unordered_map<std::string, bool> seenHazards;

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const auto& fromId = path[i];
        const auto& toId = path[i + 1];
        auto edgeOpt = graph.edgeBetween(fromId, toId);
        if (!edgeOpt) continue;
        const auto& edge = *edgeOpt;
        totalDistance += edge.distanceMeters;
        auto costOpt = RouteCostPolicy::edgeCostMeters(edge, graph.hazards);
        totalCost += costOpt.value_or(0.0);

        for (const auto& hazard : graph.hazards) {
            if (hazard.affects(fromId, toId)) {
                if (seenHazards.find(hazard.id) == seenHazards.end()) {
                    seenHazards[hazard.id] = true;
                    hazardWarnings.push_back(hazard);
                }
            }
        }
    }

    EvacuationRoute route;
    route.nodeIds = std::move(path);
    route.totalDistanceMeters = totalDistance;
    route.totalCostMeters = totalCost;
    route.hazardWarnings = std::move(hazardWarnings);
    route.estimatedDurationSeconds = estimateSeconds(totalDistance);
    return route;
}

} // namespace

double AStarPathfinder::haversineMeters(const RouteNode& from, const RouteNode& to) {
    double dLat = toRadians(to.latitude - from.latitude);
    double dLng = toRadians(to.longitude - from.longitude);
    double halfChord = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                       std::cos(toRadians(from.latitude)) * std::cos(toRadians(to.latitude)) *
                       std::sin(dLng / 2.0) * std::sin(dLng / 2.0);
    return 2.0 * EARTH_RADIUS_METERS * std::asin(std::sqrt(halfChord));
}

std::optional<EvacuationRoute> AStarPathfinder::findRoute(
    const RouteGraph& graph,
    const std::string& startId,
    const std::string& destinationId
) {
    const auto* start = graph.node(startId);
    const auto* destination = graph.node(destinationId);
    if (!start || !destination) {
        return std::nullopt;
    }

    if (startId == destinationId) {
        EvacuationRoute trivial;
        trivial.nodeIds = {startId};
        trivial.totalDistanceMeters = 0.0;
        trivial.totalCostMeters = 0.0;
        trivial.estimatedDurationSeconds = 0;
        return trivial;
    }

    std::unordered_map<std::string, double> bestCost;
    bestCost.reserve(graph.nodes.size());
    bestCost[startId] = 0.0;

    std::unordered_map<std::string, std::string> cameFrom;
    cameFrom.reserve(graph.nodes.size());

    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> open;
    open.push(SearchNode{startId, 0.0, haversineMeters(*start, *destination)});

    while (!open.empty()) {
        auto current = open.top();
        open.pop();

        auto itBest = bestCost.find(current.nodeId);
        if (itBest != bestCost.end() && current.costSoFar > itBest->second) {
            continue;
        }

        if (current.nodeId == destinationId) {
            return buildRoute(graph, cameFrom, destinationId);
        }

        for (const auto& [edge, neighbor] : graph.neighbors(current.nodeId)) {
            auto edgeCostOpt = RouteCostPolicy::edgeCostMeters(edge, graph.hazards);
            if (!edgeCostOpt) continue; // blocked edge
            double edgeCost = *edgeCostOpt;
            double nextCost = current.costSoFar + edgeCost;

            auto knownBestIt = bestCost.find(neighbor.id);
            if (knownBestIt == bestCost.end() || nextCost < knownBestIt->second) {
                bestCost[neighbor.id] = nextCost;
                cameFrom[neighbor.id] = current.nodeId;
                open.push(SearchNode{
                    neighbor.id,
                    nextCost,
                    nextCost + haversineMeters(neighbor, *destination)
                });
            }
        }
    }

    return std::nullopt; // Destination unreachable
}

} // namespace georescux::routing
