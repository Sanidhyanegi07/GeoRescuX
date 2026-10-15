#include "DynamicGraphUpdater.h"
#include <algorithm>

namespace georescux::routing {

DynamicGraphUpdater::DynamicGraphUpdater(RouteGraph& graph) : graph_(graph) {}

bool DynamicGraphUpdater::blockEdge(const std::string& fromNodeId, const std::string& toNodeId, bool bidirectional) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = graph_.updateEdgeBlocked(fromNodeId, toNodeId, true);
    if (bidirectional) {
        changed |= graph_.updateEdgeBlocked(toNodeId, fromNodeId, true);
    }
    return changed;
}

bool DynamicGraphUpdater::unblockEdge(const std::string& fromNodeId, const std::string& toNodeId, bool bidirectional) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool changed = graph_.updateEdgeBlocked(fromNodeId, toNodeId, false);
    if (bidirectional) {
        changed |= graph_.updateEdgeBlocked(toNodeId, fromNodeId, false);
    }
    return changed;
}

void DynamicGraphUpdater::addHazard(RoadHazard hazard) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Remove if existing hazard with same id
    graph_.hazards.erase(
        std::remove_if(graph_.hazards.begin(), graph_.hazards.end(), [&](const RoadHazard& h) {
            return h.id == hazard.id;
        }),
        graph_.hazards.end()
    );
    graph_.hazards.push_back(std::move(hazard));
}

bool DynamicGraphUpdater::removeHazard(const std::string& hazardId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t origSize = graph_.hazards.size();
    graph_.hazards.erase(
        std::remove_if(graph_.hazards.begin(), graph_.hazards.end(), [&](const RoadHazard& h) {
            return h.id == hazardId;
        }),
        graph_.hazards.end()
    );
    return graph_.hazards.size() < origSize;
}

size_t DynamicGraphUpdater::applyHazardPolygon(const spatial::HazardPolygon& hazardPoly, bool blockCompletely) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t affectedCount = 0;

    for (auto& edge : graph_.edges) {
        const auto* from = graph_.node(edge.fromNodeId);
        const auto* to = graph_.node(edge.toNodeId);
        if (!from || !to) continue;

        if (hazardPoly.intersectsEdge(from->latitude, from->longitude, to->latitude, to->longitude)) {
            if (blockCompletely) {
                edge.blocked = true;
            } else {
                RoadHazard h(
                    hazardPoly.getId() + "_" + edge.fromNodeId + "_" + edge.toNodeId,
                    edge.fromNodeId,
                    edge.toNodeId,
                    hazardPoly.getPenaltyCostMeters()
                );
                graph_.hazards.push_back(std::move(h));
            }
            affectedCount++;
        }
    }

    if (affectedCount > 0 && blockCompletely) {
        graph_.rebuildAdjacency();
    }

    return affectedCount;
}

} // namespace georescux::routing
