#include "ContractionHierarchies.h"
#include "RouteCostPolicy.h"
#include <algorithm>
#include <cmath>

namespace georescux::routing {

ContractionHierarchies::ContractionHierarchies(const RouteGraph& graph)
    : originalGraph_(graph) {
    for (const auto& node : originalGraph_.nodes) {
        CHNode chNode{};
        chNode.id = node.id;
        chNode.order = -1;
        chNodes_[node.id] = std::move(chNode);
    }
}

int ContractionHierarchies::computeEdgeDifference(const std::string& nodeId) const {
    const auto& neighbors = originalGraph_.neighbors(nodeId);
    const int inDegree = static_cast<int>(neighbors.size());
    const int outDegree = static_cast<int>(neighbors.size());
    const int potentialShortcuts = (inDegree * (outDegree - 1)) / 2;
    return potentialShortcuts - (inDegree + outDegree);
}

void ContractionHierarchies::contractNode(const std::string& nodeId, int order) {
    chNodes_[nodeId].order = order;
    const auto& neighbors = originalGraph_.neighbors(nodeId);

    for (size_t u = 0; u < neighbors.size(); ++u) {
        for (size_t v = u + 1; v < neighbors.size(); ++v) {
            const auto& [edgeU, nodeU] = neighbors[u];
            const auto& [edgeV, nodeV] = neighbors[v];

            if (chNodes_[nodeU.id].order != -1 || chNodes_[nodeV.id].order != -1) {
                continue; // neighbor already contracted
            }

            auto costU = RouteCostPolicy::edgeCostMeters(edgeU, originalGraph_.hazards);
            auto costV = RouteCostPolicy::edgeCostMeters(edgeV, originalGraph_.hazards);

            if (costU && costV) {
                const double shortcutWeight = *costU + *costV;

                // Add shortcut in CH graph
                ShortcutEdge sc1{nodeU.id, nodeV.id, shortcutWeight, nodeId};
                ShortcutEdge sc2{nodeV.id, nodeU.id, shortcutWeight, nodeId};

                chNodes_[nodeU.id].upwardEdges.push_back(sc1);
                chNodes_[nodeV.id].downwardEdges.push_back(sc1);

                chNodes_[nodeV.id].upwardEdges.push_back(sc2);
                chNodes_[nodeU.id].downwardEdges.push_back(sc2);

                shortcuts_.push_back(sc1);
                shortcuts_.push_back(sc2);
            }
        }
    }
}

void ContractionHierarchies::preprocess() {
    if (isPreprocessed_) return;

    // Priority queue of nodes ordered by edge difference
    using NodePriority = std::pair<int, std::string>;
    std::priority_queue<NodePriority, std::vector<NodePriority>, std::greater<NodePriority>> pq;

    for (const auto& node : originalGraph_.nodes) {
        int diff = computeEdgeDifference(node.id);
        pq.push({diff, node.id});
    }

    int currentOrder = 0;
    while (!pq.empty()) {
        auto [diff, nodeId] = pq.top();
        pq.pop();

        if (chNodes_[nodeId].order != -1) {
            continue;
        }

        // Lazy update of priority
        int newDiff = computeEdgeDifference(nodeId);
        if (!pq.empty() && newDiff > pq.top().first) {
            pq.push({newDiff, nodeId});
            continue;
        }

        contractNode(nodeId, currentOrder++);
    }

    isPreprocessed_ = true;
}

void ContractionHierarchies::unpackShortcut(const ShortcutEdge& shortcut, std::vector<std::string>& path) const {
    if (shortcut.viaNodeId.empty()) {
        path.push_back(shortcut.toNodeId);
        return;
    }

    // Recursively unpack shortcuts
    for (const auto& sc : chNodes_.at(shortcut.fromNodeId).upwardEdges) {
        if (sc.toNodeId == shortcut.viaNodeId) {
            unpackShortcut(sc, path);
            break;
        }
    }
    for (const auto& sc : chNodes_.at(shortcut.viaNodeId).upwardEdges) {
        if (sc.toNodeId == shortcut.toNodeId) {
            unpackShortcut(sc, path);
            break;
        }
    }
}

std::optional<EvacuationRoute> ContractionHierarchies::query(
    const std::string& startNodeId,
    const std::string& destinationNodeId
) const {
    if (!chNodes_.count(startNodeId) || !chNodes_.count(destinationNodeId)) {
        return std::nullopt;
    }

    if (startNodeId == destinationNodeId) {
        return EvacuationRoute{
            .nodeIds = {startNodeId},
            .totalDistanceMeters = 0.0,
            .totalCostMeters = 0.0,
            .hazardWarnings = {},
            .estimatedDurationSeconds = 0LL
        };
    }

    struct CHSearchNode {
        std::string nodeId;
        double cost;

        bool operator>(const CHSearchNode& other) const {
            return cost > other.cost;
        }
    };

    std::priority_queue<CHSearchNode, std::vector<CHSearchNode>, std::greater<CHSearchNode>> forwardQueue;
    std::unordered_map<std::string, double> forwardCost;

    std::priority_queue<CHSearchNode, std::vector<CHSearchNode>, std::greater<CHSearchNode>> backwardQueue;
    std::unordered_map<std::string, double> backwardCost;

    forwardCost[startNodeId] = 0.0;
    forwardQueue.push({startNodeId, 0.0});

    backwardCost[destinationNodeId] = 0.0;
    backwardQueue.push({destinationNodeId, 0.0});

    std::string meetingNode;
    double minCost = std::numeric_limits<double>::max();

    // Forward upward search
    while (!forwardQueue.empty()) {
        auto [u, costU] = forwardQueue.top();
        forwardQueue.pop();

        if (costU > forwardCost[u]) continue;

        const auto& chU = chNodes_.at(u);
        for (const auto& sc : chU.upwardEdges) {
            if (chNodes_.at(sc.toNodeId).order > chU.order) {
                double newCost = costU + sc.weight;
                auto it = forwardCost.find(sc.toNodeId);
                if (it == forwardCost.end() || newCost < it->second) {
                    forwardCost[sc.toNodeId] = newCost;
                    forwardQueue.push({sc.toNodeId, newCost});
                }
            }
        }
    }

    // Backward upward search
    while (!backwardQueue.empty()) {
        auto [v, costV] = backwardQueue.top();
        backwardQueue.pop();

        if (costV > backwardCost[v]) continue;

        // Check meeting node
        auto forwIt = forwardCost.find(v);
        if (forwIt != forwardCost.end()) {
            double total = forwIt->second + costV;
            if (total < minCost) {
                minCost = total;
                meetingNode = v;
            }
        }

        const auto& chV = chNodes_.at(v);
        for (const auto& sc : chV.downwardEdges) {
            if (chNodes_.at(sc.fromNodeId).order > chV.order) {
                double newCost = costV + sc.weight;
                auto it = backwardCost.find(sc.fromNodeId);
                if (it == backwardCost.end() || newCost < it->second) {
                    backwardCost[sc.fromNodeId] = newCost;
                    backwardQueue.push({sc.fromNodeId, newCost});
                }
            }
        }
    }

    if (meetingNode.empty()) {
        return std::nullopt;
    }

    return EvacuationRoute{
        .nodeIds = {startNodeId, meetingNode, destinationNodeId},
        .totalDistanceMeters = minCost,
        .totalCostMeters = minCost,
        .hazardWarnings = {},
        .estimatedDurationSeconds = static_cast<long long>(minCost / 5.0)
    };
}

} // namespace georescux::routing
