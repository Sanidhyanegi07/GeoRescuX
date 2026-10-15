#include "BidirectionalAStar.h"
#include "RouteCostPolicy.h"
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <limits>

namespace georescux::routing {

namespace {

constexpr double EARTH_RADIUS_METERS = 6371000.0;
constexpr double PI = 3.14159265358979323846;
constexpr double ASSUMED_SPEED_MPS = 5.0;
constexpr long long MIN_SECONDS = 60LL;

inline double toRadians(double deg) {
    return deg * (PI / 180.0);
}

double heuristicMeters(const RouteNode& a, const RouteNode& b) {
    const double dLat = toRadians(b.latitude - a.latitude);
    const double dLon = toRadians(b.longitude - a.longitude);
    const double lat1 = toRadians(a.latitude);
    const double lat2 = toRadians(b.latitude);

    const double sinDLat = std::sin(dLat * 0.5);
    const double sinDLon = std::sin(dLon * 0.5);

    const double h = sinDLat * sinDLat + std::cos(lat1) * std::cos(lat2) * sinDLon * sinDLon;
    const double c = 2.0 * std::asin(std::sqrt(std::min(1.0, h)));
    return EARTH_RADIUS_METERS * c;
}

struct BiSearchNode {
    std::string nodeId;
    double costSoFar{0.0};
    double priority{0.0};

    bool operator>(const BiSearchNode& other) const {
        if (std::abs(priority - other.priority) > 1e-9) {
            return priority > other.priority;
        }
        return nodeId > other.nodeId;
    }
};

long long estimateSeconds(double distanceMeters) {
    if (distanceMeters <= 0.0) return 0LL;
    long long secs = static_cast<long long>(distanceMeters / ASSUMED_SPEED_MPS);
    return std::max(secs, MIN_SECONDS);
}

} // namespace

std::optional<EvacuationRoute> BidirectionalAStar::findRoute(
    const RouteGraph& graph,
    const std::string& startNodeId,
    const std::string& destinationNodeId
) {
    const auto* startNode = graph.node(startNodeId);
    const auto* destNode = graph.node(destinationNodeId);
    if (!startNode || !destNode) {
        return std::nullopt;
    }

    if (startNodeId == destinationNodeId) {
        EvacuationRoute trivial;
        trivial.nodeIds = {startNodeId};
        trivial.totalDistanceMeters = 0.0;
        trivial.totalCostMeters = 0.0;
        trivial.estimatedDurationSeconds = 0LL;
        return trivial;
    }

    // Forward search
    std::priority_queue<BiSearchNode, std::vector<BiSearchNode>, std::greater<BiSearchNode>> forwardQueue;
    std::unordered_map<std::string, double> forwardCost;
    std::unordered_map<std::string, std::string> forwardCameFrom;

    // Backward search
    std::priority_queue<BiSearchNode, std::vector<BiSearchNode>, std::greater<BiSearchNode>> backwardQueue;
    std::unordered_map<std::string, double> backwardCost;
    std::unordered_map<std::string, std::string> backwardCameFrom;

    forwardCost[startNodeId] = 0.0;
    forwardQueue.push({startNodeId, 0.0, heuristicMeters(*startNode, *destNode)});

    backwardCost[destinationNodeId] = 0.0;
    backwardQueue.push({destinationNodeId, 0.0, heuristicMeters(*destNode, *startNode)});

    std::string meetingNode;
    double bestTotalCost = std::numeric_limits<double>::max();

    while (!forwardQueue.empty() && !backwardQueue.empty()) {
        // Forward step
        if (!forwardQueue.empty()) {
            auto currentForward = forwardQueue.top();
            forwardQueue.pop();

            auto itBestF = forwardCost.find(currentForward.nodeId);
            if (itBestF == forwardCost.end() || currentForward.costSoFar <= itBestF->second) {
                // Check if backward search has touched this node
                auto backIt = backwardCost.find(currentForward.nodeId);
                if (backIt != backwardCost.end()) {
                    double total = currentForward.costSoFar + backIt->second;
                    if (total < bestTotalCost) {
                        bestTotalCost = total;
                        meetingNode = currentForward.nodeId;
                    }
                }

                for (const auto& [edge, neighbor] : graph.neighbors(currentForward.nodeId)) {
                    auto edgeCostOpt = RouteCostPolicy::edgeCostMeters(edge, graph.hazards);
                    if (!edgeCostOpt) continue;

                    const double newCost = currentForward.costSoFar + *edgeCostOpt;
                    auto costIt = forwardCost.find(neighbor.id);
                    if (costIt == forwardCost.end() || newCost < costIt->second) {
                        forwardCost[neighbor.id] = newCost;
                        forwardCameFrom[neighbor.id] = currentForward.nodeId;

                        const double h = heuristicMeters(neighbor, *destNode);
                        forwardQueue.push({neighbor.id, newCost, newCost + h});

                        auto bIt = backwardCost.find(neighbor.id);
                        if (bIt != backwardCost.end()) {
                            double tot = newCost + bIt->second;
                            if (tot < bestTotalCost) {
                                bestTotalCost = tot;
                                meetingNode = neighbor.id;
                            }
                        }
                    }
                }
            }
        }

        // Backward step
        if (!backwardQueue.empty()) {
            auto currentBackward = backwardQueue.top();
            backwardQueue.pop();

            auto itBestB = backwardCost.find(currentBackward.nodeId);
            if (itBestB == backwardCost.end() || currentBackward.costSoFar <= itBestB->second) {
                auto forwIt = forwardCost.find(currentBackward.nodeId);
                if (forwIt != forwardCost.end()) {
                    double total = forwIt->second + currentBackward.costSoFar;
                    if (total < bestTotalCost) {
                        bestTotalCost = total;
                        meetingNode = currentBackward.nodeId;
                    }
                }

                for (const auto& [edge, neighbor] : graph.neighbors(currentBackward.nodeId)) {
                    auto edgeCostOpt = RouteCostPolicy::edgeCostMeters(edge, graph.hazards);
                    if (!edgeCostOpt) continue;

                    const double newCost = currentBackward.costSoFar + *edgeCostOpt;
                    auto costIt = backwardCost.find(neighbor.id);
                    if (costIt == backwardCost.end() || newCost < costIt->second) {
                        backwardCost[neighbor.id] = newCost;
                        backwardCameFrom[neighbor.id] = currentBackward.nodeId;

                        const double h = heuristicMeters(neighbor, *startNode);
                        backwardQueue.push({neighbor.id, newCost, newCost + h});

                        auto fIt = forwardCost.find(neighbor.id);
                        if (fIt != forwardCost.end()) {
                            double tot = fIt->second + newCost;
                            if (tot < bestTotalCost) {
                                bestTotalCost = tot;
                                meetingNode = neighbor.id;
                            }
                        }
                    }
                }
            }
        }

        if (!forwardQueue.empty() && !backwardQueue.empty()) {
            if (forwardQueue.top().priority + backwardQueue.top().priority >= bestTotalCost) {
                break;
            }
        }
    }

    if (meetingNode.empty()) {
        return std::nullopt;
    }

    // Reconstruct full path
    std::vector<std::string> path;

    // Start -> meetingNode
    std::string curr = meetingNode;
    std::vector<std::string> forwardPart;
    forwardPart.push_back(curr);
    while (curr != startNodeId) {
        auto it = forwardCameFrom.find(curr);
        if (it == forwardCameFrom.end()) break;
        curr = it->second;
        forwardPart.push_back(curr);
    }
    std::reverse(forwardPart.begin(), forwardPart.end());

    // MeetingNode -> destination
    curr = meetingNode;
    std::vector<std::string> backwardPart;
    while (curr != destinationNodeId) {
        auto it = backwardCameFrom.find(curr);
        if (it == backwardCameFrom.end()) break;
        curr = it->second;
        backwardPart.push_back(curr);
    }

    path = std::move(forwardPart);
    path.insert(path.end(), backwardPart.begin(), backwardPart.end());

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

    return EvacuationRoute{
        .nodeIds = std::move(path),
        .totalDistanceMeters = totalDistance,
        .totalCostMeters = totalCost,
        .hazardWarnings = std::move(hazardWarnings),
        .estimatedDurationSeconds = estimateSeconds(totalDistance)
    };
}

} // namespace georescux::routing
