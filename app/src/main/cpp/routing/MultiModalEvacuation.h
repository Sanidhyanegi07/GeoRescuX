#ifndef GEORESCUX_MULTI_MODAL_EVACUATION_H
#define GEORESCUX_MULTI_MODAL_EVACUATION_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <limits>

namespace georescux::routing {

enum class TransportMode : uint8_t {
    FOOT_HIKING = 1,
    EMERGENCY_VEHICLE = 2,
    RESCUE_BOAT = 3,
    HELICOPTER_AIRLIFT = 4
};

struct MultiModalNode {
    int64_t id;
    double latitude;
    double longitude;
    double elevationMeters;
    bool isTransferPoint;
    bool isHelicopterLandingZone;
    bool isHospital;
};

struct MultiModalEdge {
    int64_t fromNodeId;
    int64_t toNodeId;
    TransportMode mode;
    double lengthMeters;
    double averageSpeedMps;
    double baseDurationSeconds;
    double riskScore; // 0.0 (safe) to 1.0 (impassable/high hazard)
    bool isBlocked;
};

struct MultiModalPathLeg {
    TransportMode mode;
    int64_t startNodeId;
    int64_t endNodeId;
    std::vector<int64_t> nodePath;
    double distanceMeters;
    double durationSeconds;
    double elevationGainMeters;
};

struct MultiModalEvacuationPlan {
    std::vector<MultiModalPathLeg> legs;
    double totalDistanceMeters;
    double totalDurationSeconds;
    double totalElevationGainMeters;
    double overallRiskIndex;
    int modeTransferCount;
    bool pathFound;
};

/**
 * Multi-Modal Evacuation Routing Engine.
 * Dynamically plans composite escape routes combining Foot Hiking, 4x4 Emergency Vehicles,
 * Rescue Inflatable Boats across swollen rivers, and Helicopter Airlift corridors.
 */
class MultiModalEvacuationEngine {
public:
    MultiModalEvacuationEngine() = default;

    void addNode(const MultiModalNode& node);
    void addEdge(const MultiModalEdge& edge);
    void blockEdge(int64_t fromId, int64_t toId, TransportMode mode);
    void clearBlockedEdges();

    /**
     * Finds the Pareto-optimal fastest evacuation plan from startNodeId to any safe haven / hospital.
     * Evaluates mode transfer penalties (boarding, staging, triage prep).
     */
    [[nodiscard]] MultiModalEvacuationPlan findEvacuationRoute(
        int64_t startNodeId,
        int64_t targetNodeId,
        bool allowHelicopter = false,
        bool allowBoat = true,
        double maxWalkSlopeDeg = 35.0
    ) const;

    /**
     * Finds the fastest route from startNodeId to the nearest reachable hospital.
     */
    [[nodiscard]] MultiModalEvacuationPlan findRouteToNearestHospital(
        int64_t startNodeId,
        bool allowHelicopter = true
    ) const;

    [[nodiscard]] size_t getNodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] size_t getEdgeCount() const noexcept;

private:
    std::unordered_map<int64_t, MultiModalNode> nodes_;
    std::unordered_map<int64_t, std::vector<MultiModalEdge>> adjacency_;

    static double getTransferPenaltySeconds(TransportMode from, TransportMode to) noexcept;
};

} // namespace georescux::routing

#endif // GEORESCUX_MULTI_MODAL_EVACUATION_H
