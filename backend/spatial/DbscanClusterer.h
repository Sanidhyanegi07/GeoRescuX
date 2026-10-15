#ifndef GEORESCUX_BACKEND_DBSCAN_CLUSTERER_H
#define GEORESCUX_BACKEND_DBSCAN_CLUSTERER_H

#include <vector>
#include <string>
#include <cstdint>

namespace georescux::backend::spatial {

struct DistressPoint {
    std::string id;
    double latitude;
    double longitude;
    uint8_t urgencyScore; // 0-100 from triage
    uint16_t peopleCount;
    int clusterId{-1};    // -1 = unclassified, 0 = noise, >0 = cluster ID
};

struct DisasterCluster {
    int clusterId;
    double centroidLat;
    double centroidLon;
    size_t totalPoints;
    uint32_t totalPeopleCount;
    double averageUrgencyScore;
    uint8_t maxUrgencyScore;
    double radiusMeters;
    std::vector<std::string> distressIds;
};

/**
 * High-performance DBSCAN (Density-Based Spatial Clustering of Applications with Noise)
 * algorithm specifically tailored for disaster distress beacons.
 * Clusters thousands of individual SOS pings into macroscopic disaster zones.
 */
class DbscanClusterer {
public:
    /**
     * @param epsilonMeters Neighborhood search radius in meters (e.g. 500m)
     * @param minPoints Minimum number of points to form a dense disaster cluster
     */
    DbscanClusterer(double epsilonMeters = 500.0, size_t minPoints = 3);

    /**
     * Performs DBSCAN clustering over a set of distress points.
     * Returns a list of identified macro disaster clusters.
     */
    std::vector<DisasterCluster> cluster(std::vector<DistressPoint>& points) const;

    /**
     * Calculates great-circle Haversine distance in meters.
     */
    static double haversineMeters(double lat1, double lon1, double lat2, double lon2) noexcept;

private:
    double epsilonMeters_;
    size_t minPoints_;

    std::vector<size_t> regionQuery(
        const std::vector<DistressPoint>& points,
        size_t targetIndex
    ) const;

    void expandCluster(
        std::vector<DistressPoint>& points,
        size_t pointIndex,
        std::vector<size_t>& neighborIndices,
        int clusterId
    ) const;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_DBSCAN_CLUSTERER_H
