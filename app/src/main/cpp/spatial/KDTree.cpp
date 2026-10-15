#include "KDTree.h"
#include "GeodesicMath.h"
#include <algorithm>
#include <queue>
#include <limits>

namespace georescux::spatial {

void KDTree::clear() noexcept {
    root_.reset();
    size_ = 0;
}

void KDTree::build(std::vector<SpatialPoint> points) {
    clear();
    if (points.empty()) return;
    size_ = points.size();
    root_ = buildRecursive(points, 0, points.size(), 0);
}

std::unique_ptr<KDNode> KDTree::buildRecursive(
    std::vector<SpatialPoint>& pts,
    size_t start, size_t end,
    int depth
) {
    if (start >= end) return nullptr;

    const int axis = depth % 2;
    const size_t mid = start + (end - start) / 2;

    auto comparator = [axis](const SpatialPoint& a, const SpatialPoint& b) {
        return axis == 0 ? (a.latitude < b.latitude) : (a.longitude < b.longitude);
    };

    std::nth_element(pts.begin() + start, pts.begin() + mid, pts.begin() + end, comparator);

    auto node = std::make_unique<KDNode>(pts[mid], axis);
    node->left = buildRecursive(pts, start, mid, depth + 1);
    node->right = buildRecursive(pts, mid + 1, end, depth + 1);

    return node;
}

std::optional<SpatialPoint> KDTree::findNearest(
    double targetLat, double targetLon,
    bool safeHavenOnly
) const {
    if (!root_) return std::nullopt;

    const SpatialPoint* bestPoint = nullptr;
    double bestDist = std::numeric_limits<double>::max();

    nearestNeighborSearch(root_.get(), targetLat, targetLon, safeHavenOnly, bestPoint, bestDist);

    if (bestPoint) {
        return *bestPoint;
    }
    return std::nullopt;
}

void KDTree::nearestNeighborSearch(
    const KDNode* node,
    double targetLat, double targetLon,
    bool safeHavenOnly,
    const SpatialPoint*& bestPoint,
    double& bestDistMeters
) const {
    if (!node) return;

    if (!safeHavenOnly || node->point.isSafeHaven) {
        const double dist = GeodesicMath::haversineDistanceMeters(
            targetLat, targetLon,
            node->point.latitude, node->point.longitude
        );
        if (dist < bestDistMeters) {
            bestDistMeters = dist;
            bestPoint = &node->point;
        }
    }

    const int axis = node->axis;
    const double nodeCoord = (axis == 0) ? node->point.latitude : node->point.longitude;
    const double targetCoord = (axis == 0) ? targetLat : targetLon;

    const KDNode* first = (targetCoord < nodeCoord) ? node->left.get() : node->right.get();
    const KDNode* second = (targetCoord < nodeCoord) ? node->right.get() : node->left.get();

    nearestNeighborSearch(first, targetLat, targetLon, safeHavenOnly, bestPoint, bestDistMeters);

    // Approximate angular distance to splitting plane in meters
    // 1 deg latitude ~ 111,320m; longitude scaled by cos(latitude)
    double planeDistMeters = 0.0;
    if (axis == 0) {
        planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0;
    } else {
        const double latRad = GeodesicMath::toRadians(targetLat);
        planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0 * std::cos(latRad);
    }

    if (planeDistMeters < bestDistMeters) {
        nearestNeighborSearch(second, targetLat, targetLon, safeHavenOnly, bestPoint, bestDistMeters);
    }
}

std::vector<SpatialPoint> KDTree::findKNearest(
    double targetLat, double targetLon,
    size_t k,
    bool safeHavenOnly
) const {
    if (!root_ || k == 0) return {};

    using QueueEntry = std::pair<double, const SpatialPoint*>;
    auto comp = [](const QueueEntry& a, const QueueEntry& b) {
        return a.first < b.first; // max-heap
    };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, decltype(comp)> maxHeap(comp);

    std::function<void(const KDNode*)> searchK = [&](const KDNode* node) {
        if (!node) return;

        if (!safeHavenOnly || node->point.isSafeHaven) {
            const double dist = GeodesicMath::haversineDistanceMeters(
                targetLat, targetLon,
                node->point.latitude, node->point.longitude
            );
            if (maxHeap.size() < k) {
                maxHeap.push({dist, &node->point});
            } else if (dist < maxHeap.top().first) {
                maxHeap.pop();
                maxHeap.push({dist, &node->point});
            }
        }

        const int axis = node->axis;
        const double nodeCoord = (axis == 0) ? node->point.latitude : node->point.longitude;
        const double targetCoord = (axis == 0) ? targetLat : targetLon;

        const KDNode* first = (targetCoord < nodeCoord) ? node->left.get() : node->right.get();
        const KDNode* second = (targetCoord < nodeCoord) ? node->right.get() : node->left.get();

        searchK(first);

        double planeDistMeters = 0.0;
        if (axis == 0) {
            planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0;
        } else {
            const double latRad = GeodesicMath::toRadians(targetLat);
            planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0 * std::cos(latRad);
        }

        if (maxHeap.size() < k || planeDistMeters < maxHeap.top().first) {
            searchK(second);
        }
    };

    searchK(root_.get());

    std::vector<SpatialPoint> results;
    results.reserve(maxHeap.size());
    while (!maxHeap.empty()) {
        results.push_back(*maxHeap.top().second);
        maxHeap.pop();
    }
    std::reverse(results.begin(), results.end());
    return results;
}

std::vector<SpatialPoint> KDTree::findWithinRadius(
    double targetLat, double targetLon,
    double radiusMeters,
    bool safeHavenOnly
) const {
    std::vector<SpatialPoint> results;
    if (!root_ || radiusMeters <= 0.0) return results;

    radiusSearch(root_.get(), targetLat, targetLon, radiusMeters, safeHavenOnly, results);
    return results;
}

void KDTree::radiusSearch(
    const KDNode* node,
    double targetLat, double targetLon,
    double radiusMeters,
    bool safeHavenOnly,
    std::vector<SpatialPoint>& results
) const {
    if (!node) return;

    if (!safeHavenOnly || node->point.isSafeHaven) {
        const double dist = GeodesicMath::haversineDistanceMeters(
            targetLat, targetLon,
            node->point.latitude, node->point.longitude
        );
        if (dist <= radiusMeters) {
            results.push_back(node->point);
        }
    }

    const int axis = node->axis;
    const double nodeCoord = (axis == 0) ? node->point.latitude : node->point.longitude;
    const double targetCoord = (axis == 0) ? targetLat : targetLon;

    const KDNode* first = (targetCoord < nodeCoord) ? node->left.get() : node->right.get();
    const KDNode* second = (targetCoord < nodeCoord) ? node->right.get() : node->left.get();

    radiusSearch(first, targetLat, targetLon, radiusMeters, safeHavenOnly, results);

    double planeDistMeters = 0.0;
    if (axis == 0) {
        planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0;
    } else {
        const double latRad = GeodesicMath::toRadians(targetLat);
        planeDistMeters = std::abs(targetCoord - nodeCoord) * 111320.0 * std::cos(latRad);
    }

    if (planeDistMeters <= radiusMeters) {
        radiusSearch(second, targetLat, targetLon, radiusMeters, safeHavenOnly, results);
    }
}

} // namespace georescux::spatial
