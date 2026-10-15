#include "DynamicTollRiskRouter.h"
#include <queue>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace georescux::routing {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_M = 6371008.8;

double haversineMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_M * c;
}
} // namespace

void DynamicTollRiskRouter::addNode(int64_t nodeId, double lat, double lon, double elevation) {
    nodes_[nodeId] = {nodeId, lat, lon, elevation};
}

void DynamicTollRiskRouter::addEdge(const DynamicRiskEdge& edge) {
    adjacency_[edge.fromNode].push_back(edge);
    if (!edge.isOneWay) {
        DynamicRiskEdge reverseEdge = edge;
        reverseEdge.fromNode = edge.toNode;
        reverseEdge.toNode = edge.fromNode;
        adjacency_[edge.toNode].push_back(reverseEdge);
    }
}

double DynamicTollRiskRouter::calculateBearingDegrees(
    double lat1, double lon1,
    double lat2, double lon2
) noexcept {
    double phi1 = lat1 * DEG2RAD;
    double phi2 = lat2 * DEG2RAD;
    double dLambda = (lon2 - lon1) * DEG2RAD;

    double y = std::sin(dLambda) * std::cos(phi2);
    double x = std::cos(phi1) * std::sin(phi2) -
               std::sin(phi1) * std::cos(phi2) * std::cos(dLambda);

    double theta = std::atan2(y, x) * RAD2DEG;
    return std::fmod(theta + 360.0, 360.0);
}

NavigationManeuverType DynamicTollRiskRouter::classifyTurn(double turnAngleDeg) noexcept {
    // Normalize to [-180, 180]
    while (turnAngleDeg > 180.0) turnAngleDeg -= 360.0;
    while (turnAngleDeg < -180.0) turnAngleDeg += 360.0;

    if (std::abs(turnAngleDeg) <= 15.0) return NavigationManeuverType::CONTINUE_STRAIGHT;
    if (turnAngleDeg > 15.0 && turnAngleDeg <= 45.0) return NavigationManeuverType::SLIGHT_RIGHT;
    if (turnAngleDeg > 45.0 && turnAngleDeg <= 120.0) return NavigationManeuverType::TURN_RIGHT;
    if (turnAngleDeg > 120.0 && turnAngleDeg <= 165.0) return NavigationManeuverType::SHARP_RIGHT;
    if (turnAngleDeg < -15.0 && turnAngleDeg >= -45.0) return NavigationManeuverType::SLIGHT_LEFT;
    if (turnAngleDeg < -45.0 && turnAngleDeg >= -120.0) return NavigationManeuverType::TURN_LEFT;
    if (turnAngleDeg < -120.0 && turnAngleDeg >= -165.0) return NavigationManeuverType::SHARP_LEFT;
    return NavigationManeuverType::U_TURN;
}

struct SearchNode {
    int64_t nodeId;
    double priority; // f = g + h
    double costG;
    double durationG;
    double distanceG;
    double logSurvivalG;

    bool operator>(const SearchNode& o) const {
        return priority > o.priority;
    }
};

DynamicRiskRouteResult DynamicTollRiskRouter::findSafestRoute(
    int64_t startNode,
    int64_t endNode,
    double riskAversionFactor
) const {
    DynamicRiskRouteResult result{};
    result.isPathFound = false;

    if (nodes_.find(startNode) == nodes_.end() || nodes_.find(endNode) == nodes_.end()) {
        return result;
    }

    const auto& targetNode = nodes_.at(endNode);

    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> pq;
    std::unordered_map<int64_t, double> bestCost;
    std::unordered_map<int64_t, int64_t> predecessor;

    std::unordered_map<int64_t, double> distMap;
    std::unordered_map<int64_t, double> durMap;
    std::unordered_map<int64_t, double> logSurvMap;

    bestCost[startNode] = 0.0;
    distMap[startNode] = 0.0;
    durMap[startNode] = 0.0;
    logSurvMap[startNode] = 0.0;

    pq.push({startNode, 0.0, 0.0, 0.0, 0.0, 0.0});

    bool reached = false;

    while (!pq.empty()) {
        auto current = pq.top();
        pq.pop();

        if (current.nodeId == endNode) {
            reached = true;
            break;
        }

        if (current.costG > bestCost[current.nodeId]) {
            continue;
        }

        auto adjIt = adjacency_.find(current.nodeId);
        if (adjIt == adjacency_.end()) continue;

        for (const auto& edge : adjIt->second) {
            double speedMps = std::max(2.0, (edge.baseSpeedKmh * 1000.0) / 3600.0);
            double legDuration = edge.lengthMeters / speedMps;

            // Risk penalty: -ln(1 - p)
            double safeProb = std::clamp(1.0 - edge.breachProbability, 0.01, 1.0);
            double riskCost = -std::log(safeProb) * (riskAversionFactor * 1200.0);

            double legTotalCost = legDuration + riskCost;
            double nextG = current.costG + legTotalCost;

            if (bestCost.find(edge.toNode) == bestCost.end() || nextG < bestCost[edge.toNode]) {
                bestCost[edge.toNode] = nextG;
                predecessor[edge.toNode] = current.nodeId;

                distMap[edge.toNode] = current.distanceG + edge.lengthMeters;
                durMap[edge.toNode] = current.durationG + legDuration;
                logSurvMap[edge.toNode] = current.logSurvivalG + std::log(safeProb);

                const auto& nextNode = nodes_.at(edge.toNode);
                double hDist = haversineMeters(nextNode.lat, nextNode.lon, targetNode.lat, targetNode.lon);
                double hTime = hDist / 20.0; // max ~72 km/h admissible heuristic

                pq.push({
                    edge.toNode,
                    nextG + hTime,
                    nextG,
                    durMap[edge.toNode],
                    distMap[edge.toNode],
                    logSurvMap[edge.toNode]
                });
            }
        }
    }

    if (!reached) {
        return result;
    }

    // Backtrack path
    std::vector<int64_t> path;
    int64_t curr = endNode;
    while (curr != startNode) {
        path.push_back(curr);
        curr = predecessor[curr];
    }
    path.push_back(startNode);
    std::reverse(path.begin(), path.end());

    result.nodePath = path;
    result.totalDistanceMeters = distMap[endNode];
    result.totalDurationSeconds = durMap[endNode];
    result.routeSurvivalProbability = std::exp(logSurvMap[endNode]);
    result.turnByTurnInstructions = generateNavigationGuidance(path);
    result.isPathFound = true;

    return result;
}

std::vector<NavigationStep> DynamicTollRiskRouter::generateNavigationGuidance(
    const std::vector<int64_t>& nodePath
) const {
    if (nodePath.size() < 2) return {};

    std::vector<NavigationStep> steps;

    // First step: Depart
    const auto& start = nodes_.at(nodePath[0]);
    const auto& next = nodes_.at(nodePath[1]);
    double initialBearing = calculateBearingDegrees(start.lat, start.lon, next.lat, next.lon);

    steps.push_back({
        NavigationManeuverType::DEPART,
        "Depart and head toward your evacuation destination",
        "Connecting Route",
        haversineMeters(start.lat, start.lon, next.lat, next.lon),
        15.0,
        start.lat,
        start.lon
    });

    double prevBearing = initialBearing;

    for (size_t i = 1; i < nodePath.size() - 1; ++i) {
        const auto& pA = nodes_.at(nodePath[i - 1]);
        const auto& pB = nodes_.at(nodePath[i]);
        const auto& pC = nodes_.at(nodePath[i + 1]);

        double inBearing = calculateBearingDegrees(pA.lat, pA.lon, pB.lat, pB.lon);
        double outBearing = calculateBearingDegrees(pB.lat, pB.lon, pC.lat, pC.lon);
        double turnAngle = outBearing - inBearing;

        NavigationManeuverType maneuver = classifyTurn(turnAngle);
        double legDist = haversineMeters(pB.lat, pB.lon, pC.lat, pC.lon);

        if (maneuver != NavigationManeuverType::CONTINUE_STRAIGHT) {
            std::ostringstream ss;
            switch (maneuver) {
                case NavigationManeuverType::TURN_LEFT: ss << "Turn left"; break;
                case NavigationManeuverType::TURN_RIGHT: ss << "Turn right"; break;
                case NavigationManeuverType::SLIGHT_LEFT: ss << "Bear slightly left"; break;
                case NavigationManeuverType::SLIGHT_RIGHT: ss << "Bear slightly right"; break;
                case NavigationManeuverType::SHARP_LEFT: ss << "Sharp left turn"; break;
                case NavigationManeuverType::SHARP_RIGHT: ss << "Sharp right turn"; break;
                case NavigationManeuverType::U_TURN: ss << "Make a U-turn"; break;
                default: ss << "Continue"; break;
            }

            // Find road name from edge if available
            std::string road = "Evacuation Route";
            auto it = adjacency_.find(nodePath[i]);
            if (it != adjacency_.end()) {
                for (const auto& e : it->second) {
                    if (e.toNode == nodePath[i + 1] && !e.roadName.empty()) {
                        road = e.roadName;
                        break;
                    }
                }
            }

            ss << " onto " << road;

            steps.push_back({
                maneuver,
                ss.str(),
                road,
                legDist,
                legDist / 8.33, // ~30 km/h mountain speed
                pB.lat,
                pB.lon
            });
        }
        prevBearing = outBearing;
    }

    // Final arrival step
    const auto& last = nodes_.at(nodePath.back());
    steps.push_back({
        NavigationManeuverType::ARRIVE_DESTINATION,
        "You have arrived at the safe haven shelter",
        "Safe Haven",
        0.0,
        0.0,
        last.lat,
        last.lon
    });

    return steps;
}

} // namespace georescux::routing
