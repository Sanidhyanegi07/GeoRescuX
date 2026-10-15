#include "DbscanClusterer.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace georescux::backend::spatial {

namespace {
constexpr double EARTH_RADIUS_METERS = 6371008.8;
constexpr double PI = 3.14159265358979323846;

inline double toRadians(double deg) noexcept {
    return deg * (PI / 180.0);
}
} // namespace

DbscanClusterer::DbscanClusterer(double epsilonMeters, size_t minPoints)
    : epsilonMeters_(std::max(10.0, epsilonMeters)),
      minPoints_(std::max<size_t>(1, minPoints)) {}

double DbscanClusterer::haversineMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    const double phi1 = toRadians(lat1);
    const double phi2 = toRadians(lat2);
    const double deltaPhi = toRadians(lat2 - lat1);
    const double deltaLambda = toRadians(lon2 - lon1);

    const double sDPhi = std::sin(deltaPhi * 0.5);
    const double sDLambda = std::sin(deltaLambda * 0.5);

    const double a = sDPhi * sDPhi + std::cos(phi1) * std::cos(phi2) * sDLambda * sDLambda;
    const double clampedA = std::clamp(a, 0.0, 1.0);
    return 2.0 * EARTH_RADIUS_METERS * std::asin(std::sqrt(clampedA));
}

std::vector<size_t> DbscanClusterer::regionQuery(
    const std::vector<DistressPoint>& points,
    size_t targetIndex
) const {
    std::vector<size_t> neighbors;
    const auto& target = points[targetIndex];

    for (size_t i = 0; i < points.size(); ++i) {
        if (haversineMeters(target.latitude, target.longitude, points[i].latitude, points[i].longitude) <= epsilonMeters_) {
            neighbors.push_back(i);
        }
    }
    return neighbors;
}

void DbscanClusterer::expandCluster(
    std::vector<DistressPoint>& points,
    size_t pointIndex,
    std::vector<size_t>& neighborIndices,
    int clusterId
) const {
    points[pointIndex].clusterId = clusterId;

    for (size_t i = 0; i < neighborIndices.size(); ++i) {
        const size_t currentNeighborIdx = neighborIndices[i];
        auto& neighbor = points[currentNeighborIdx];

        if (neighbor.clusterId == 0) {
            // Previously marked as noise, now part of border
            neighbor.clusterId = clusterId;
        }

        if (neighbor.clusterId != -1) {
            continue; // Already processed
        }

        neighbor.clusterId = clusterId;
        std::vector<size_t> secondaryNeighbors = regionQuery(points, currentNeighborIdx);

        if (secondaryNeighbors.size() >= minPoints_) {
            for (size_t snIdx : secondaryNeighbors) {
                if (std::find(neighborIndices.begin(), neighborIndices.end(), snIdx) == neighborIndices.end()) {
                    neighborIndices.push_back(snIdx);
                }
            }
        }
    }
}

std::vector<DisasterCluster> DbscanClusterer::cluster(std::vector<DistressPoint>& points) const {
    int currentClusterId = 0;

    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].clusterId != -1) {
            continue; // Already classified
        }

        std::vector<size_t> neighbors = regionQuery(points, i);
        if (neighbors.size() < minPoints_) {
            points[i].clusterId = 0; // Mark as noise
        } else {
            currentClusterId++;
            expandCluster(points, i, neighbors, currentClusterId);
        }
    }

    // Aggregate statistics per cluster
    std::unordered_map<int, std::vector<size_t>> clusterGroups;
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].clusterId > 0) {
            clusterGroups[points[i].clusterId].push_back(i);
        }
    }

    std::vector<DisasterCluster> results;
    results.reserve(clusterGroups.size());

    for (const auto& [cId, indices] : clusterGroups) {
        double sumLat = 0.0;
        double sumLon = 0.0;
        uint32_t totalPeople = 0;
        double sumUrgency = 0.0;
        uint8_t maxUrgency = 0;
        std::vector<std::string> ids;
        ids.reserve(indices.size());

        for (size_t idx : indices) {
            const auto& pt = points[idx];
            sumLat += pt.latitude;
            sumLon += pt.longitude;
            totalPeople += pt.peopleCount;
            sumUrgency += pt.urgencyScore;
            maxUrgency = std::max(maxUrgency, pt.urgencyScore);
            ids.push_back(pt.id);
        }

        const double centroidLat = sumLat / indices.size();
        const double centroidLon = sumLon / indices.size();

        double maxRadius = 0.0;
        for (size_t idx : indices) {
            const double d = haversineMeters(centroidLat, centroidLon, points[idx].latitude, points[idx].longitude);
            maxRadius = std::max(maxRadius, d);
        }

        results.push_back(DisasterCluster{
            .clusterId = cId,
            .centroidLat = centroidLat,
            .centroidLon = centroidLon,
            .totalPoints = indices.size(),
            .totalPeopleCount = totalPeople,
            .averageUrgencyScore = sumUrgency / indices.size(),
            .maxUrgencyScore = maxUrgency,
            .radiusMeters = maxRadius,
            .distressIds = std::move(ids)
        });
    }

    // Sort clusters by highest urgency and people count
    std::sort(results.begin(), results.end(), [](const DisasterCluster& a, const DisasterCluster& b) {
        if (a.maxUrgencyScore != b.maxUrgencyScore) {
            return a.maxUrgencyScore > b.maxUrgencyScore;
        }
        return a.totalPeopleCount > b.totalPeopleCount;
    });

    return results;
}

} // namespace georescux::backend::spatial
