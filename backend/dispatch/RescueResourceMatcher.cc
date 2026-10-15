#include "RescueResourceMatcher.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::dispatch {

double RescueResourceMatcher::calculateCapabilityWeight(RescueUnitType unitType, uint8_t maxUrgencyScore) noexcept {
    // Critical life-threatening emergencies (urgency > 85) require Medical Ambulances or Air Evac
    if (maxUrgencyScore >= 85) {
        switch (unitType) {
            case RescueUnitType::AIR_EVAC_HELICOPTER: return 2.0;
            case RescueUnitType::MEDICAL_AMBULANCE: return 1.8;
            case RescueUnitType::NDRF_SEARCH_RESCUE: return 1.5;
            default: return 1.0;
        }
    } else if (maxUrgencyScore >= 50) {
        switch (unitType) {
            case RescueUnitType::NDRF_SEARCH_RESCUE: return 1.8;
            case RescueUnitType::BOAT_FLOOD_RESCUE: return 1.6;
            case RescueUnitType::MEDICAL_AMBULANCE: return 1.4;
            default: return 1.0;
        }
    }
    return 1.0;
}

std::vector<DispatchAssignment> RescueResourceMatcher::matchUnitsToClusters(
    std::vector<RescueUnit>& units,
    const std::vector<spatial::DisasterCluster>& clusters
) {
    std::vector<DispatchAssignment> assignments;
    if (units.empty() || clusters.empty()) return assignments;

    struct CandidateMatch {
        size_t unitIndex;
        size_t clusterIndex;
        double distanceKm;
        double etaMinutes;
        double costScore;
    };

    std::vector<CandidateMatch> candidates;

    for (size_t u = 0; u < units.size(); ++u) {
        if (!units[u].isAvailable) continue;

        for (size_t c = 0; c < clusters.size(); ++c) {
            const double distMeters = spatial::DbscanClusterer::haversineMeters(
                units[u].currentLat, units[u].currentLon,
                clusters[c].centroidLat, clusters[c].centroidLon
            );
            const double distKm = distMeters / 1000.0;
            const double speed = std::max(10.0, units[u].operationalSpeedKmh);
            const double etaMinutes = (distKm / speed) * 60.0;

            const double capabilityMultiplier = calculateCapabilityWeight(units[u].type, clusters[c].maxUrgencyScore);

            // Cost formula: lower cost is better
            // Prioritize high urgency, close distance, fast ETA, and capability
            const double urgencyFactor = 100.0 / std::max(1.0, static_cast<double>(clusters[c].maxUrgencyScore));
            const double cost = (etaMinutes * urgencyFactor) / capabilityMultiplier;

            candidates.push_back(CandidateMatch{
                .unitIndex = u,
                .clusterIndex = c,
                .distanceKm = distKm,
                .etaMinutes = etaMinutes,
                .costScore = cost
            });
        }
    }

    // Sort candidate matches by lowest cost (best match first)
    std::sort(candidates.begin(), candidates.end(), [](const CandidateMatch& a, const CandidateMatch& b) {
        return a.costScore < b.costScore;
    });

    std::vector<bool> unitAssigned(units.size(), false);
    std::vector<bool> clusterSatisfied(clusters.size(), false);

    for (const auto& match : candidates) {
        if (unitAssigned[match.unitIndex] || clusterSatisfied[match.clusterIndex]) {
            continue;
        }

        unitAssigned[match.unitIndex] = true;
        clusterSatisfied[match.clusterIndex] = true;
        units[match.unitIndex].isAvailable = false;

        const auto& unit = units[match.unitIndex];
        const auto& cluster = clusters[match.clusterIndex];

        assignments.push_back(DispatchAssignment{
            .unitId = unit.unitId,
            .unitName = unit.unitName,
            .targetClusterId = cluster.clusterId,
            .targetLat = cluster.centroidLat,
            .targetLon = cluster.centroidLon,
            .estimatedDistanceKm = match.distanceKm,
            .estimatedEtaMinutes = match.etaMinutes,
            .peopleCovered = std::min<uint32_t>(unit.capacityPeople, cluster.totalPeopleCount),
            .matchScore = 100.0 / (1.0 + match.costScore)
        });
    }

    return assignments;
}

} // namespace georescux::backend::dispatch
