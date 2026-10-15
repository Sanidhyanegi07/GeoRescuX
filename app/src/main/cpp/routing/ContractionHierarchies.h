#ifndef GEORESCUX_ROUTING_CONTRACTION_HIERARCHIES_H
#define GEORESCUX_ROUTING_CONTRACTION_HIERARCHIES_H

#include "RouteGraph.h"
#include "EvacuationRoute.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <limits>
#include <optional>

namespace georescux::routing {

struct ShortcutEdge {
    std::string fromNodeId;
    std::string toNodeId;
    double weight;
    std::string viaNodeId; // intermediate contracted node
};

struct CHNode {
    std::string id;
    int order{-1}; // contraction order (0 = contracted first, higher = higher hierarchy)
    std::vector<ShortcutEdge> upwardEdges;
    std::vector<ShortcutEdge> downwardEdges;
};

/**
 * Contraction Hierarchies (CH) high-performance routing acceleration engine.
 * Precomputes node ordering and shortcut edges to achieve sub-millisecond
 * evacuation queries across large regional graphs (Uttarakhand, Himachal Pradesh).
 */
class ContractionHierarchies {
public:
    explicit ContractionHierarchies(const RouteGraph& graph);

    /**
     * Preprocesses the graph by contracting nodes in order of edge difference.
     */
    void preprocess();

    /**
     * Executes bidirectional upward search in sub-millisecond query time.
     */
    [[nodiscard]] std::optional<EvacuationRoute> query(
        const std::string& startNodeId,
        const std::string& destinationNodeId
    ) const;

    [[nodiscard]] bool isPreprocessed() const noexcept { return isPreprocessed_; }
    [[nodiscard]] size_t getShortcutCount() const noexcept { return shortcuts_.size(); }

private:
    const RouteGraph& originalGraph_;
    bool isPreprocessed_{false};
    std::unordered_map<std::string, CHNode> chNodes_;
    std::vector<ShortcutEdge> shortcuts_;

    int computeEdgeDifference(const std::string& nodeId) const;
    void contractNode(const std::string& nodeId, int order);
    void unpackShortcut(const ShortcutEdge& shortcut, std::vector<std::string>& path) const;
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_CONTRACTION_HIERARCHIES_H
