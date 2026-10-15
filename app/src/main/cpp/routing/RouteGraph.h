#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "RouteNode.h"
#include "RouteEdge.h"
#include "RoadHazard.h"

namespace georescux::routing {

class RouteGraph {
public:
    std::vector<RouteNode> nodes;
    std::vector<RouteEdge> edges;
    std::vector<RoadHazard> hazards;
    int version{0};

    RouteGraph() = default;
    RouteGraph(std::vector<RouteNode> n, std::vector<RouteEdge> e,
               std::vector<RoadHazard> h = {}, int ver = 0);

    const RouteNode* node(const std::string& nodeId) const;
    const std::vector<std::pair<RouteEdge, RouteNode>>& neighbors(const std::string& nodeId) const;
    std::optional<RouteEdge> edgeBetween(const std::string& fromNodeId, const std::string& toNodeId) const;

    void rebuildAdjacency();
    bool updateEdgeBlocked(const std::string& fromNodeId, const std::string& toNodeId, bool blocked);

private:
    std::unordered_map<std::string, RouteNode> nodesById;
    std::unordered_map<std::string, std::vector<std::pair<RouteEdge, RouteNode>>> adjacency;
    static const std::vector<std::pair<RouteEdge, RouteNode>> emptyNeighbors;

    void buildIndexAndAdjacency();
};

} // namespace georescux::routing
