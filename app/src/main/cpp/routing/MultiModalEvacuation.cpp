#include "MultiModalEvacuation.h"
#include <queue>
#include <algorithm>
#include <cmath>

namespace georescux::routing {

void MultiModalEvacuationEngine::addNode(const MultiModalNode& node) {
    nodes_[node.id] = node;
}

void MultiModalEvacuationEngine::addEdge(const MultiModalEdge& edge) {
    adjacency_[edge.fromNodeId].push_back(edge);
}

void MultiModalEvacuationEngine::blockEdge(int64_t fromId, int64_t toId, TransportMode mode) {
    auto it = adjacency_.find(fromId);
    if (it != adjacency_.end()) {
        for (auto& edge : it->second) {
            if (edge.toNodeId == toId && edge.mode == mode) {
                edge.isBlocked = true;
            }
        }
    }
}

void MultiModalEvacuationEngine::clearBlockedEdges() {
    for (auto& [fromId, edges] : adjacency_) {
        for (auto& edge : edges) {
            edge.isBlocked = false;
        }
    }
}

size_t MultiModalEvacuationEngine::getEdgeCount() const noexcept {
    size_t count = 0;
    for (const auto& [_, edges] : adjacency_) {
        count += edges.size();
    }
    return count;
}

double MultiModalEvacuationEngine::getTransferPenaltySeconds(TransportMode from, TransportMode to) noexcept {
    if (from == to) return 0.0;

    if (to == TransportMode::HELICOPTER_AIRLIFT) {
        return 600.0; // 10 minutes: HLZ staging, rotor spin-up, casualty strapping
    }
    if (to == TransportMode::RESCUE_BOAT) {
        return 300.0; // 5 minutes: raft inflation, life vests, river embarkation
    }
    if (to == TransportMode::EMERGENCY_VEHICLE) {
        return 180.0; // 3 minutes: ambulance boarding
    }
    return 60.0; // Transfer to foot hiking
}

struct SearchState {
    int64_t nodeId;
    TransportMode mode;
    double cost; // weighted seconds
    double realDuration;
    double totalDistance;
    double totalElevationGain;
    double maxRisk;

    bool operator>(const SearchState& other) const {
        return cost > other.cost;
    }
};

struct PredecessorInfo {
    int64_t prevNodeId = -1;
    TransportMode prevMode = TransportMode::FOOT_HIKING;
    MultiModalEdge usedEdge{};
    double legDuration = 0.0;
    double legDistance = 0.0;
    double legElevGain = 0.0;
};

MultiModalEvacuationPlan MultiModalEvacuationEngine::findEvacuationRoute(
    int64_t startNodeId,
    int64_t targetNodeId,
    bool allowHelicopter,
    bool allowBoat,
    double maxWalkSlopeDeg
) const {
    MultiModalEvacuationPlan plan{};
    plan.pathFound = false;

    if (nodes_.find(startNodeId) == nodes_.end() || nodes_.find(targetNodeId) == nodes_.end()) {
        return plan;
    }

    // Hash key for (nodeId, mode)
    auto stateKey = [](int64_t nid, TransportMode m) -> uint64_t {
        return (static_cast<uint64_t>(nid) << 8) | static_cast<uint8_t>(m);
    };

    std::priority_queue<SearchState, std::vector<SearchState>, std::greater<SearchState>> pq;
    std::unordered_map<uint64_t, double> bestCost;
    std::unordered_map<uint64_t, PredecessorInfo> predecessor;

    // Start in foot mode
    uint64_t startKey = stateKey(startNodeId, TransportMode::FOOT_HIKING);
    bestCost[startKey] = 0.0;
    pq.push({startNodeId, TransportMode::FOOT_HIKING, 0.0, 0.0, 0.0, 0.0, 0.0});

    SearchState finalState{};
    bool reached = false;

    while (!pq.empty()) {
        auto current = pq.top();
        pq.pop();

        if (current.nodeId == targetNodeId) {
            finalState = current;
            reached = true;
            break;
        }

        uint64_t currKey = stateKey(current.nodeId, current.mode);
        if (current.cost > bestCost[currKey]) {
            continue;
        }

        const auto& fromNode = nodes_.at(current.nodeId);
        auto adjIt = adjacency_.find(current.nodeId);
        if (adjIt == adjacency_.end()) continue;

        for (const auto& edge : adjIt->second) {
            if (edge.isBlocked) continue;

            // Mode compatibility checks
            if (edge.mode == TransportMode::HELICOPTER_AIRLIFT && (!allowHelicopter || !fromNode.isHelicopterLandingZone)) {
                continue;
            }
            if (edge.mode == TransportMode::RESCUE_BOAT && !allowBoat) {
                continue;
            }

            const auto& toNode = nodes_.at(edge.toNodeId);

            // Slope evaluation for foot hiking
            double elevDiff = toNode.elevationMeters - fromNode.elevationMeters;
            double elevGain = std::max(0.0, elevDiff);

            if (edge.mode == TransportMode::FOOT_HIKING && edge.lengthMeters > 0.0) {
                double slopeRad = std::atan(std::abs(elevDiff) / edge.lengthMeters);
                double slopeDeg = slopeRad * (180.0 / 3.141592653589793);
                if (slopeDeg > maxWalkSlopeDeg) {
                    continue; // trail too steep for safe foot evacuation
                }
            }

            // Calculate duration with Tobler slope adjustment for foot
            double edgeDuration = edge.baseDurationSeconds;
            if (edge.mode == TransportMode::FOOT_HIKING && edge.lengthMeters > 0.0) {
                double slope = elevDiff / edge.lengthMeters;
                // Tobler's hiking speed factor: W = 6 * exp(-3.5 * |s + 0.05|)
                double speedFactor = std::exp(-3.5 * std::abs(slope + 0.05));
                speedFactor = std::clamp(speedFactor, 0.15, 1.5);
                edgeDuration /= speedFactor;
            }

            // Mode transfer penalty
            double transferDelay = getTransferPenaltySeconds(current.mode, edge.mode);
            double totalLegTime = edgeDuration + transferDelay;

            // Weighted cost incorporating hazard risk
            double riskPenalty = edge.riskScore * 600.0; // up to 10 min penalty for risky paths
            double newCost = current.cost + totalLegTime + riskPenalty;

            uint64_t nextKey = stateKey(edge.toNodeId, edge.mode);
            if (bestCost.find(nextKey) == bestCost.end() || newCost < bestCost[nextKey]) {
                bestCost[nextKey] = newCost;
                predecessor[nextKey] = {
                    current.nodeId,
                    current.mode,
                    edge,
                    totalLegTime,
                    edge.lengthMeters,
                    elevGain
                };

                pq.push({
                    edge.toNodeId,
                    edge.mode,
                    newCost,
                    current.realDuration + totalLegTime,
                    current.totalDistance + edge.lengthMeters,
                    current.totalElevationGain + elevGain,
                    std::max(current.maxRisk, edge.riskScore)
                });
            }
        }
    }

    if (!reached) {
        return plan;
    }

    // Reconstruct path
    std::vector<PredecessorInfo> pathEdges;
    int64_t traceNode = finalState.nodeId;
    TransportMode traceMode = finalState.mode;

    while (traceNode != startNodeId) {
        uint64_t k = stateKey(traceNode, traceMode);
        auto it = predecessor.find(k);
        if (it == predecessor.end()) break;

        pathEdges.push_back(it->second);
        traceNode = it->second.prevNodeId;
        traceMode = it->second.prevMode;
    }

    std::reverse(pathEdges.begin(), pathEdges.end());

    // Aggregate into distinct multi-modal legs
    if (!pathEdges.empty()) {
        MultiModalPathLeg curLeg;
        curLeg.mode = pathEdges.front().usedEdge.mode;
        curLeg.startNodeId = pathEdges.front().prevNodeId;
        curLeg.nodePath.push_back(pathEdges.front().prevNodeId);
        curLeg.distanceMeters = 0.0;
        curLeg.durationSeconds = 0.0;
        curLeg.elevationGainMeters = 0.0;

        for (const auto& pe : pathEdges) {
            if (pe.usedEdge.mode != curLeg.mode) {
                // Leg transition
                curLeg.endNodeId = curLeg.nodePath.back();
                plan.legs.push_back(curLeg);

                curLeg = MultiModalPathLeg{};
                curLeg.mode = pe.usedEdge.mode;
                curLeg.startNodeId = pe.prevNodeId;
                curLeg.nodePath.push_back(pe.prevNodeId);
                plan.modeTransferCount++;
            }

            curLeg.nodePath.push_back(pe.usedEdge.toNodeId);
            curLeg.distanceMeters += pe.legDistance;
            curLeg.durationSeconds += pe.legDuration;
            curLeg.elevationGainMeters += pe.legElevGain;
        }

        curLeg.endNodeId = curLeg.nodePath.back();
        plan.legs.push_back(curLeg);
    }

    plan.totalDistanceMeters = finalState.totalDistance;
    plan.totalDurationSeconds = finalState.realDuration;
    plan.totalElevationGainMeters = finalState.totalElevationGain;
    plan.overallRiskIndex = finalState.maxRisk;
    plan.pathFound = true;

    return plan;
}

MultiModalEvacuationPlan MultiModalEvacuationEngine::findRouteToNearestHospital(
    int64_t startNodeId,
    bool allowHelicopter
) const {
    MultiModalEvacuationPlan bestPlan{};
    double minDuration = std::numeric_limits<double>::infinity();

    for (const auto& [nodeId, node] : nodes_) {
        if (node.isHospital && nodeId != startNodeId) {
            auto plan = findEvacuationRoute(startNodeId, nodeId, allowHelicopter);
            if (plan.pathFound && plan.totalDurationSeconds < minDuration) {
                minDuration = plan.totalDurationSeconds;
                bestPlan = plan;
            }
        }
    }

    return bestPlan;
}

} // namespace georescux::routing
