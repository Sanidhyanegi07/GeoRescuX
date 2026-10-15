#include "RescueDispatchOptimizer.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <iostream>

namespace georescux::backend::dispatch {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double EARTH_RADIUS_KM = 6371.0088;

double toRadians(double deg) noexcept {
    return deg * (PI / 180.0);
}
} // namespace

double RescueDispatchOptimizer::haversineDistanceKm(
    double lat1, double lon1,
    double lat2, double lon2
) noexcept {
    double dLat = toRadians(lat2 - lat1);
    double dLon = toRadians(lon2 - lon1);
    double rLat1 = toRadians(lat1);
    double rLat2 = toRadians(lat2);

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_KM * c;
}

double RescueDispatchOptimizer::estimateTravelTimeSec(
    double lat1, double lon1,
    double lat2, double lon2,
    double avgSpeedKmh
) noexcept {
    if (avgSpeedKmh <= 0.1) avgSpeedKmh = 30.0; // default mountain vehicle speed
    double distKm = haversineDistanceKm(lat1, lon1, lat2, lon2);
    // Multiply by 1.25 road winding factor for mountain roads
    double roadDistKm = distKm * 1.25;
    return (roadDistKm / avgSpeedKmh) * 3600.0;
}

double RescueDispatchOptimizer::calculateRouteCost(const VehicleRoute& route, double maxMissionTimeSec) {
    double cost = route.totalDistanceKm * 1.0; // distance penalty
    if (route.totalDurationSec > maxMissionTimeSec) {
        cost += (route.totalDurationSec - maxMissionTimeSec) * 100.0; // heavy overtime penalty
    }
    cost += route.totalPenalty;
    return cost;
}

VehicleRoute RescueDispatchOptimizer::localSearch2Opt(
    VehicleRoute route, 
    double avgSpeedKmh, 
    double maxMissionTimeSec
) {
    if (route.visitedTargets.size() <= 2) {
        return route;
    }

    bool improved = true;
    int maxIterations = 50;
    int iteration = 0;

    while (improved && iteration++ < maxIterations) {
        improved = false;
        double bestDuration = route.totalDurationSec;
        size_t n = route.visitedTargets.size();

        for (size_t i = 0; i < n - 1; ++i) {
            for (size_t k = i + 1; k < n; ++k) {
                // Reverse sub-segment [i, k]
                std::vector<RescueVictimTarget> newTargets = route.visitedTargets;
                std::reverse(newTargets.begin() + i, newTargets.begin() + k + 1);

                // Recalculate duration & distance
                double newDist = 0.0;
                double newDuration = 0.0;
                double currentTime = 0.0;
                double penalty = 0.0;
                bool timeWindowViolated = false;

                double prevLat = route.visitedTargets.front().latitude;
                double prevLon = route.visitedTargets.front().longitude;

                for (const auto& target : newTargets) {
                    double legDist = haversineDistanceKm(prevLat, prevLon, target.latitude, target.longitude) * 1.25;
                    double legTime = (legDist / avgSpeedKmh) * 3600.0;
                    currentTime += legTime;
                    newDist += legDist;

                    // Arrived early: wait until window start
                    if (currentTime < target.timeWindowStartSec) {
                        currentTime = target.timeWindowStartSec;
                    }

                    // Arrived late: penalty proportional to triage urgency
                    if (currentTime > target.timeWindowEndSec) {
                        double lateness = currentTime - target.timeWindowEndSec;
                        double urgencyMultiplier = (target.triageCategory == TriageUrgency::RED_IMMEDIATE) ? 50.0 : 10.0;
                        penalty += lateness * urgencyMultiplier;
                        if (lateness > 3600.0 && target.triageCategory == TriageUrgency::RED_IMMEDIATE) {
                            timeWindowViolated = true;
                        }
                    }

                    currentTime += target.serviceDurationSec;
                    prevLat = target.latitude;
                    prevLon = target.longitude;
                }
                newDuration = currentTime;

                if (!timeWindowViolated && newDuration + penalty < bestDuration) {
                    route.visitedTargets = std::move(newTargets);
                    route.totalDistanceKm = newDist;
                    route.totalDurationSec = newDuration;
                    route.totalPenalty = penalty;
                    bestDuration = newDuration + penalty;
                    improved = true;
                    break;
                }
            }
            if (improved) break;
        }
    }

    return route;
}

DispatchSolution RescueDispatchOptimizer::optimizeDispatch(
    const std::vector<RescueDepot>& depots,
    const std::vector<RescueVehicle>& fleet,
    const std::vector<RescueVictimTarget>& victims,
    double maxMissionTimeSec
) {
    DispatchSolution solution;
    solution.totalMissionDurationSec = 0.0;
    solution.totalDistanceKm = 0.0;
    solution.totalTriageScore = 0.0;
    solution.solutionQualityCost = 0.0;

    if (fleet.empty() || victims.empty()) {
        solution.unassignedVictims = victims;
        return solution;
    }

    // Sort victims by triage urgency descending (RED first), then by earliest deadline
    std::vector<RescueVictimTarget> sortedVictims = victims;
    std::sort(sortedVictims.begin(), sortedVictims.end(), [](const RescueVictimTarget& a, const RescueVictimTarget& b) {
        if (a.triageCategory != b.triageCategory) {
            return a.triageCategory > b.triageCategory; // High urgency first
        }
        return a.timeWindowEndSec < b.timeWindowEndSec; // Earlier deadline first
    });

    // Map depot ID to Depot
    std::unordered_map<std::string, RescueDepot> depotMap;
    for (const auto& d : depots) {
        depotMap[d.id] = d;
    }

    // Initialize vehicle routes
    std::vector<VehicleRoute> routes;
    routes.reserve(fleet.size());
    for (const auto& v : fleet) {
        VehicleRoute r;
        r.vehicleId = v.id;
        r.totalDistanceKm = 0.0;
        r.totalDurationSec = 0.0;
        r.totalPenalty = 0.0;
        r.isFeasible = true;
        routes.push_back(r);
    }

    // Track vehicle capacities & current location
    struct VehicleState {
        RescueVehicle vehicle;
        double currentLat;
        double currentLon;
        double currentTimeSec;
        int remainingPatients;
        int remainingStretchers;
    };

    std::vector<VehicleState> vehicleStates;
    vehicleStates.reserve(fleet.size());
    for (const auto& v : fleet) {
        double startLat = v.currentLatitude;
        double startLon = v.currentLongitude;
        if (std::abs(startLat) < 1e-4 && depotMap.find(v.depotId) != depotMap.end()) {
            startLat = depotMap[v.depotId].latitude;
            startLon = depotMap[v.depotId].longitude;
        }
        vehicleStates.push_back({v, startLat, startLon, 0.0, v.capacityPatients, v.capacityStretchers});
    }

    // Assign casualties using greedy insertion with triage & deadline weighting
    for (const auto& victim : sortedVictims) {
        int bestVehicleIndex = -1;
        double bestInsertionCost = std::numeric_limits<double>::infinity();

        for (size_t i = 0; i < vehicleStates.size(); ++i) {
            auto& state = vehicleStates[i];

            // Capacity constraints
            if (state.remainingPatients < victim.patientCount) continue;
            if (state.remainingStretchers < victim.requiredStretcherCount) continue;

            // Helicopter check: Helicopters only for remote / critical red victims
            if (state.vehicle.type == VehicleType::RESCUE_HELICOPTER && 
                victim.triageCategory < TriageUrgency::YELLOW_DELAYED) {
                continue; // Preserve air assets for red/critical yellow
            }

            // ALS check: Red victims should get ALS if available
            if (victim.triageCategory == TriageUrgency::RED_IMMEDIATE && 
                !state.vehicle.hasAdvancedLifeSupport && 
                state.vehicle.type != VehicleType::RESCUE_HELICOPTER) {
                // Allowed only if no ALS available, but penalize
            }

            double legDist = haversineDistanceKm(state.currentLat, state.currentLon, victim.latitude, victim.longitude) * 1.25;
            
            // Check operating radius
            if (state.vehicle.maxOperatingRadiusKm > 0.0 && legDist > state.vehicle.maxOperatingRadiusKm) {
                continue;
            }

            double legTime = (legDist / state.vehicle.avgSpeedKmh) * 3600.0;
            double arrivalTime = state.currentTimeSec + legTime;

            // Calculate cost
            double cost = legDist * 1.5;
            if (arrivalTime > victim.timeWindowEndSec) {
                double lateness = arrivalTime - victim.timeWindowEndSec;
                cost += lateness * (victim.triageCategory == TriageUrgency::RED_IMMEDIATE ? 500.0 : 50.0);
            }
            if (arrivalTime + victim.serviceDurationSec > maxMissionTimeSec) {
                cost += 50000.0; // severe overtime
            }

            if (cost < bestInsertionCost) {
                bestInsertionCost = cost;
                bestVehicleIndex = static_cast<int>(i);
            }
        }

        if (bestVehicleIndex != -1 && bestInsertionCost < 40000.0) {
            auto& state = vehicleStates[bestVehicleIndex];
            auto& route = routes[bestVehicleIndex];

            double legDist = haversineDistanceKm(state.currentLat, state.currentLon, victim.latitude, victim.longitude) * 1.25;
            double legTime = (legDist / state.vehicle.avgSpeedKmh) * 3600.0;
            state.currentTimeSec += legTime;
            if (state.currentTimeSec < victim.timeWindowStartSec) {
                state.currentTimeSec = victim.timeWindowStartSec;
            }
            state.currentTimeSec += victim.serviceDurationSec;
            state.currentLat = victim.latitude;
            state.currentLon = victim.longitude;
            state.remainingPatients -= victim.patientCount;
            state.remainingStretchers -= victim.requiredStretcherCount;

            route.visitedTargets.push_back(victim);
            route.totalDistanceKm += legDist;
            route.totalDurationSec = state.currentTimeSec;

            solution.totalTriageScore += (victim.triageCategory + 1) * victim.patientCount;
        } else {
            solution.unassignedVictims.push_back(victim);
        }
    }

    // Apply 2-opt post-optimization for each route
    for (size_t i = 0; i < routes.size(); ++i) {
        if (!routes[i].visitedTargets.empty()) {
            routes[i] = localSearch2Opt(routes[i], vehicleStates[i].vehicle.avgSpeedKmh, maxMissionTimeSec);
            solution.totalDistanceKm += routes[i].totalDistanceKm;
            solution.totalMissionDurationSec = std::max(solution.totalMissionDurationSec, routes[i].totalDurationSec);
            solution.solutionQualityCost += calculateRouteCost(routes[i], maxMissionTimeSec);
        }
    }

    solution.routes = std::move(routes);
    return solution;
}

} // namespace georescux::backend::dispatch
