#ifndef GEORESCUX_DYNAMIC_TOLL_RISK_ROUTER_H
#define GEORESCUX_DYNAMIC_TOLL_RISK_ROUTER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace georescux::routing {

enum class NavigationManeuverType : uint8_t {
    DEPART,
    CONTINUE_STRAIGHT,
    SLIGHT_LEFT,
    SLIGHT_RIGHT,
    TURN_LEFT,
    TURN_RIGHT,
    SHARP_LEFT,
    SHARP_RIGHT,
    U_TURN,
    ENTER_HAZARD_ZONE,
    EXIT_HAZARD_ZONE,
    ARRIVE_DESTINATION
};

struct NavigationStep {
    NavigationManeuverType maneuver;
    std::string instructionText;
    std::string roadName;
    double stepDistanceMeters;
    double stepDurationSeconds;
    double startLatitude;
    double startLongitude;
};

struct DynamicRiskEdge {
    int64_t fromNode;
    int64_t toNode;
    std::string roadName;
    double lengthMeters;
    double baseSpeedKmh;
    double breachProbability; // 0.0 to 1.0 (hazard risk)
    double slopeDegrees;
    bool isOneWay;
};

struct DynamicRiskRouteResult {
    std::vector<int64_t> nodePath;
    std::vector<NavigationStep> turnByTurnInstructions;
    double totalDistanceMeters;
    double totalDurationSeconds;
    double routeSurvivalProbability; // Product of (1 - breachProbability)
    bool isPathFound;
};

/**
 * Dynamic Time-Dependent Risk Router and Turn-by-Turn Maneuver Generator.
 * Calculates safest reliable evacuation paths under stochastic edge failure risks,
 * and synthesizes turn-by-turn navigation instructions for disaster responders.
 */
class DynamicTollRiskRouter {
public:
    DynamicTollRiskRouter() = default;

    void addNode(int64_t nodeId, double lat, double lon, double elevation = 0.0);
    void addEdge(const DynamicRiskEdge& edge);

    /**
     * Finds the safest reliable route from startNode to endNode.
     * @param riskAversionFactor 0.0 (fastest route ignoring risk) to 1.0 (safest route)
     */
    [[nodiscard]] DynamicRiskRouteResult findSafestRoute(
        int64_t startNode,
        int64_t endNode,
        double riskAversionFactor = 0.5
    ) const;

    /**
     * Synthesizes turn-by-turn verbal navigation instructions from a node path.
     */
    [[nodiscard]] std::vector<NavigationStep> generateNavigationGuidance(
        const std::vector<int64_t>& nodePath
    ) const;

private:
    struct RouterNode {
        int64_t id;
        double lat;
        double lon;
        double elevation;
    };

    std::unordered_map<int64_t, RouterNode> nodes_;
    std::unordered_map<int64_t, std::vector<DynamicRiskEdge>> adjacency_;

    static double calculateBearingDegrees(double lat1, double lon1, double lat2, double lon2) noexcept;
    static NavigationManeuverType classifyTurn(double turnAngleDeg) noexcept;
};

} // namespace georescux::routing

#endif // GEORESCUX_DYNAMIC_TOLL_RISK_ROUTER_H
