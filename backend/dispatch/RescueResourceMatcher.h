#ifndef GEORESCUX_BACKEND_RESCUE_RESOURCE_MATCHER_H
#define GEORESCUX_BACKEND_RESCUE_RESOURCE_MATCHER_H

#include "../spatial/DbscanClusterer.h"
#include <vector>
#include <string>
#include <cstdint>

namespace georescux::backend::dispatch {

enum class RescueUnitType {
    NDRF_SEARCH_RESCUE,
    MEDICAL_AMBULANCE,
    AIR_EVAC_HELICOPTER,
    FIRE_DISASTER_TRUCK,
    BOAT_FLOOD_RESCUE
};

struct RescueUnit {
    std::string unitId;
    std::string unitName;
    RescueUnitType type;
    double currentLat;
    double currentLon;
    uint16_t capacityPeople;
    double operationalSpeedKmh;
    bool isAvailable{true};
};

struct DispatchAssignment {
    std::string unitId;
    std::string unitName;
    int targetClusterId;
    double targetLat;
    double targetLon;
    double estimatedDistanceKm;
    double estimatedEtaMinutes;
    uint32_t peopleCovered;
    double matchScore;
};

/**
 * Dispatch algorithm for allocating scarce rescue units to disaster clusters.
 * Solves bipartite matching using multi-criteria optimization:
 * minimizes response time (ETA) while maximizing triage urgency coverage and capability fit.
 */
class RescueResourceMatcher {
public:
    static std::vector<DispatchAssignment> matchUnitsToClusters(
        std::vector<RescueUnit>& units,
        const std::vector<spatial::DisasterCluster>& clusters
    );

private:
    static double calculateCapabilityWeight(RescueUnitType unitType, uint8_t maxUrgencyScore) noexcept;
};

} // namespace georescux::backend::dispatch

#endif // GEORESCUX_BACKEND_RESCUE_RESOURCE_MATCHER_H
