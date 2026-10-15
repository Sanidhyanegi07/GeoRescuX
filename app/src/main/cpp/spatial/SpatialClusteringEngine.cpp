#include "SpatialClusteringEngine.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double EARTH_RADIUS_M = 6371008.8;

double haversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_M * c;
}

double crossProduct(const Point2D& o, const Point2D& a, const Point2D& b) noexcept {
    return (a.longitude - o.longitude) * (b.latitude - o.latitude) -
           (a.latitude - o.latitude) * (b.longitude - o.longitude);
}

// Disjoint-Set Union (DSU) for Kruskal's MST clustering
struct Dsu {
    std::vector<int> parent;
    std::vector<int> rank;

    Dsu(int n) : parent(n), rank(n, 0) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }

    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }

    void unite(int i, int j) {
        int rootI = find(i);
        int rootJ = find(j);
        if (rootI != rootJ) {
            if (rank[rootI] < rank[rootJ]) {
                parent[rootI] = rootJ;
            } else if (rank[rootI] > rank[rootJ]) {
                parent[rootJ] = rootI;
            } else {
                parent[rootJ] = rootI;
                rank[rootI]++;
            }
        }
    }
};

struct Edge {
    int u, v;
    double dist;
    bool operator<(const Edge& o) const {
        return dist < o.dist;
    }
};
} // namespace

std::vector<Point2D> SpatialClusteringEngine::computeConvexHull(
    const std::vector<Point2D>& points
) {
    size_t n = points.size();
    if (n <= 2) return points;

    std::vector<Point2D> sorted = points;
    std::sort(sorted.begin(), sorted.end(), [](const Point2D& a, const Point2D& b) {
        if (std::abs(a.latitude - b.latitude) > 1e-9) {
            return a.latitude < b.latitude;
        }
        return a.longitude < b.longitude;
    });

    // Remove duplicates
    sorted.erase(std::unique(sorted.begin(), sorted.end(), [](const Point2D& a, const Point2D& b) {
        return std::abs(a.latitude - b.latitude) < 1e-9 && std::abs(a.longitude - b.longitude) < 1e-9;
    }), sorted.end());

    n = sorted.size();
    if (n <= 2) return sorted;

    std::vector<Point2D> hull(2 * n);
    size_t k = 0;

    // Build lower hull
    for (size_t i = 0; i < n; ++i) {
        while (k >= 2 && crossProduct(hull[k - 2], hull[k - 1], sorted[i]) <= 0.0) {
            k--;
        }
        hull[k++] = sorted[i];
    }

    // Build upper hull
    for (size_t i = n - 1, t = k + 1; i > 0; --i) {
        while (k >= t && crossProduct(hull[k - 2], hull[k - 1], sorted[i - 1]) <= 0.0) {
            k--;
        }
        hull[k++] = sorted[i - 1];
    }

    hull.resize(k - 1);
    return hull;
}

void SpatialClusteringEngine::computeClusterGeometry(
    const std::vector<IncidentPoint>& members,
    Point2D& outCentroid,
    double& outRadiusMeters
) noexcept {
    if (members.empty()) {
        outCentroid = {0.0, 0.0};
        outRadiusMeters = 0.0;
        return;
    }

    double sumLat = 0.0;
    double sumLon = 0.0;
    for (const auto& m : members) {
        sumLat += m.latitude;
        sumLon += m.longitude;
    }

    outCentroid.latitude = sumLat / members.size();
    outCentroid.longitude = sumLon / members.size();

    double maxDist = 0.0;
    for (const auto& m : members) {
        double d = haversineDistanceMeters(outCentroid.latitude, outCentroid.longitude, m.latitude, m.longitude);
        if (d > maxDist) maxDist = d;
    }

    outRadiusMeters = maxDist;
}

std::vector<MacroIncidentCluster> SpatialClusteringEngine::clusterIncidents(
    const std::vector<IncidentPoint>& points,
    double maxClusterDistanceMeters,
    size_t minPointsPerCluster
) {
    if (points.empty()) return {};

    int n = static_cast<int>(points.size());
    Dsu dsu(n);

    // Build complete or thresholded edge list
    std::vector<Edge> edges;
    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dist = haversineDistanceMeters(
                points[i].latitude, points[i].longitude,
                points[j].latitude, points[j].longitude
            );
            if (dist <= maxClusterDistanceMeters) {
                edges.push_back({i, j, dist});
            }
        }
    }

    // Unite points within distance threshold
    for (const auto& edge : edges) {
        dsu.unite(edge.u, edge.v);
    }

    // Group members by cluster root
    std::unordered_map<int, std::vector<IncidentPoint>> clusterGroups;
    for (int i = 0; i < n; ++i) {
        int root = dsu.find(i);
        clusterGroups[root].push_back(points[i]);
    }

    // Assemble MacroIncidentCluster objects
    std::vector<MacroIncidentCluster> clusters;
    int nextClusterId = 1;

    for (const auto& [_, members] : clusterGroups) {
        if (members.size() < minPointsPerCluster) continue;

        MacroIncidentCluster c{};
        c.clusterId = nextClusterId++;
        c.totalVictims = static_cast<int>(members.size());
        c.criticalRedCount = 0;
        c.aggregateSeverityScore = 0.0;
        c.memberPoints = members;

        std::vector<Point2D> pointList;
        pointList.reserve(members.size());

        for (const auto& m : members) {
            if (m.triagePriority == 3) c.criticalRedCount++;
            c.aggregateSeverityScore += m.severityWeight * m.triagePriority;
            pointList.push_back({m.latitude, m.longitude});
        }

        SpatialClusteringEngine::computeClusterGeometry(members, c.centroid, c.radiusMeters);
        c.convexHullPerimeter = SpatialClusteringEngine::computeConvexHull(pointList);

        clusters.push_back(std::move(c));
    }

    return clusters;
}

} // namespace georescux::spatial
