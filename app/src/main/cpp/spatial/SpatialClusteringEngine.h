#ifndef GEORESCUX_SPATIAL_CLUSTERING_ENGINE_H
#define GEORESCUX_SPATIAL_CLUSTERING_ENGINE_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "HazardPolygon.h"

namespace georescux::spatial {

struct IncidentPoint {
    int64_t id;
    double latitude;
    double longitude;
    int triagePriority; // 1=Green, 2=Yellow, 3=Red
    double severityWeight;
};

struct MacroIncidentCluster {
    int clusterId;
    Point2D centroid;
    double radiusMeters;
    int totalVictims;
    int criticalRedCount;
    double aggregateSeverityScore;
    std::vector<IncidentPoint> memberPoints;
    std::vector<Point2D> convexHullPerimeter;
};

/**
 * Client-Side Spatial Clustering and Convex Hull Engine for Disaster Mapping.
 * Performs fast spatial partitioning using MST and Hierarchical Agglomerative Clustering (HAC),
 * and computes 2D Monotone Chain convex hulls for rendering hazard envelopes on map canvases.
 */
class SpatialClusteringEngine {
public:
    SpatialClusteringEngine() = default;

    /**
     * Clusters incident points within maxClusterDistanceMeters into macro incident clusters.
     */
    [[nodiscard]] static std::vector<MacroIncidentCluster> clusterIncidents(
        const std::vector<IncidentPoint>& points,
        double maxClusterDistanceMeters = 500.0,
        size_t minPointsPerCluster = 1
    );

    /**
     * Computes the 2D Convex Hull of a set of points using Andrew's Monotone Chain algorithm.
     */
    [[nodiscard]] static std::vector<Point2D> computeConvexHull(
        const std::vector<Point2D>& points
    );

    /**
     * Computes the centroid and spatial variance of a set of GPS points.
     */
    static void computeClusterGeometry(
        const std::vector<IncidentPoint>& members,
        Point2D& outCentroid,
        double& outRadiusMeters
    ) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_CLUSTERING_ENGINE_H
