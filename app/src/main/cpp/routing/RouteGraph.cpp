#include "RouteGraph.h"

namespace georescux::routing {

const std::vector<std::pair<RouteEdge, RouteNode>> RouteGraph::emptyNeighbors = {};

RouteGraph::RouteGraph(std::vector<RouteNode> n, std::vector<RouteEdge> e,
                       std::vector<RoadHazard> h, int ver)
    : nodes(std::move(n)), edges(std::move(e)), hazards(std::move(h)), version(ver) {
    buildIndexAndAdjacency();
}

void RouteGraph::buildIndexAndAdjacency() {
    nodesById.clear();
    adjacency.clear();

    for (const auto& nd : nodes) {
        nodesById.emplace(nd.id, nd);
    }

    for (const auto& edge : edges) {
        auto fromIt = nodesById.find(edge.fromNodeId);
        auto toIt = nodesById.find(edge.toNodeId);
        if (fromIt != nodesById.end() && toIt != nodesById.end()) {
            adjacency[edge.fromNodeId].emplace_back(edge, toIt->second);
            adjacency[edge.toNodeId].emplace_back(edge, fromIt->second);
        }
    }
}

const RouteNode* RouteGraph::node(const std::string& nodeId) const {
    auto it = nodesById.find(nodeId);
    return it != nodesById.end() ? &(it->second) : nullptr;
}

const std::vector<std::pair<RouteEdge, RouteNode>>& RouteGraph::neighbors(const std::string& nodeId) const {
    auto it = adjacency.find(nodeId);
    return it != adjacency.end() ? it->second : emptyNeighbors;
}

std::optional<RouteEdge> RouteGraph::edgeBetween(const std::string& fromNodeId, const std::string& toNodeId) const {
    for (const auto& edge : edges) {
        if (edge.connects(fromNodeId, toNodeId)) {
            return edge;
        }
    }
    return std::nullopt;
}

} // namespace georescux::routing
