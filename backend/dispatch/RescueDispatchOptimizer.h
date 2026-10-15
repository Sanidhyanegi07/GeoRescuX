#ifndef GEORESCUX_BACKEND_RESCUE_DISPATCH_OPTIMIZER_H
#define GEORESCUX_BACKEND_RESCUE_DISPATCH_OPTIMIZER_H

#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <memory>

namespace georescux::backend::dispatch {

enum class VehicleType {
    BLS_AMBULANCE,       // Basic Life Support
    ALS_AMBULANCE,       // Advanced Life Support (ICU on wheels)
    HEAVY_RESCUE_TRUCK,  // Hydraulic shears, extrication gear
    ALL_TERRAIN_4X4,     // Rough mountain roads, narrow paths
    RESCUE_HELICOPTER    // Evacuation for critical red-triage
};

struct TriageUrgency {
    static constexpr int BLACK_DECEASED = 0;
    static constexpr int GREEN_MINOR = 1;
    static constexpr int YELLOW_DELAYED = 2;
    static constexpr int RED_IMMEDIATE = 3;
};

struct RescueVictimTarget {
    std::string id;
    double latitude;
    double longitude;
    int triageCategory; // 0=Black, 1=Green, 2=Yellow, 3=Red
    int patientCount;
    int requiredStretcherCount;
    double timeWindowStartSec;
    double timeWindowEndSec;   // Maximum survival or critical intervention window
    double serviceDurationSec; // On-scene stabilization time
};

struct RescueDepot {
    std::string id;
    std::string name;
    double latitude;
    double longitude;
    int availableVehicles;
};

struct RescueVehicle {
    std::string id;
    std::string depotId;
    VehicleType type;
    int capacityPatients;
    int capacityStretchers;
    double maxOperatingRadiusKm;
    double avgSpeedKmh;
    double currentLatitude;
    double currentLongitude;
    bool hasAdvancedLifeSupport;
};

struct VehicleRoute {
    std::string vehicleId;
    std::vector<RescueVictimTarget> visitedTargets;
    double totalDistanceKm;
    double totalDurationSec;
    double totalPenalty;
    bool isFeasible;
};

struct DispatchSolution {
    std::vector<VehicleRoute> routes;
    std::vector<RescueVictimTarget> unassignedVictims;
    double totalMissionDurationSec;
    double totalDistanceKm;
    double totalTriageScore;
    double solutionQualityCost;
};

/**
 * Multi-Depot Multi-Vehicle Routing Problem with Time Windows (MDVRPTW)
 * specifically engineered for Disaster and Mass-Casualty Response.
 * Implements Clarke-Wright heuristic followed by Simulated Annealing and 2-Opt local search.
 */
class RescueDispatchOptimizer {
public:
    RescueDispatchOptimizer() = default;

    /**
     * Solves the emergency fleet dispatch plan given active victims, depots, and vehicles.
     * Prioritizes RED triage casualties within critical physiological time windows.
     */
    static DispatchSolution optimizeDispatch(
        const std::vector<RescueDepot>& depots,
        const std::vector<RescueVehicle>& fleet,
        const std::vector<RescueVictimTarget>& victims,
        double maxMissionTimeSec = 14400.0 // 4 hours
    );

    /**
     * Calculates Geodesic Haversine travel time in seconds between two points.
     */
    static double estimateTravelTimeSec(
        double lat1, double lon1,
        double lat2, double lon2,
        double avgSpeedKmh
    ) noexcept;

    /**
     * Calculates Haversine distance in kilometers.
     */
    static double haversineDistanceKm(
        double lat1, double lon1,
        double lat2, double lon2
    ) noexcept;

private:
    static double calculateRouteCost(const VehicleRoute& route, double maxMissionTimeSec);
    static VehicleRoute localSearch2Opt(VehicleRoute route, double avgSpeedKmh, double maxMissionTimeSec);
};

} // namespace georescux::backend::dispatch

#endif // GEORESCUX_BACKEND_RESCUE_DISPATCH_OPTIMIZER_H
