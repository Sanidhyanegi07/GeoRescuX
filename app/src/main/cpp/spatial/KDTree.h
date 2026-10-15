#ifndef GEORESCUX_SPATIAL_KDTREE_H
#define GEORESCUX_SPATIAL_KDTREE_H

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>

namespace georescux::spatial {

struct SpatialPoint {
    std::string id;
    double latitude;
    double longitude;
    double elevationMeters{0.0};
    bool isSafeHaven{false};
};

struct KDNode {
    SpatialPoint point;
    std::unique_ptr<KDNode> left;
    std::unique_ptr<KDNode> right;
    int axis{0}; // 0 = latitude, 1 = longitude

    explicit KDNode(SpatialPoint pt, int ax = 0)
        : point(std::move(pt)), left(nullptr), right(nullptr), axis(ax) {}
};

/**
 * 2D KD-Tree optimized for geographic coordinates.
 * Provides logarithmic nearest neighbor search and range/radius queries
 * to locate nearest evacuation nodes, safe havens, and hazard clusters in real-time.
 */
class KDTree {
public:
    KDTree() = default;
    ~KDTree() = default;

    // Movable but non-copyable for performance
    KDTree(KDTree&&) noexcept = default;
    KDTree& operator=(KDTree&&) noexcept = default;
    KDTree(const KDTree&) = delete;
    KDTree& operator=(const KDTree&) = delete;

    /**
     * Builds a balanced 2D KD-Tree from a list of spatial points.
     */
    void build(std::vector<SpatialPoint> points);

    /**
     * Clears all nodes in the tree.
     */
    void clear() noexcept;

    /**
     * Returns true if tree contains zero nodes.
     */
    [[nodiscard]] bool empty() const noexcept { return root_ == nullptr; }

    /**
     * Returns the total count of indexed nodes.
     */
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /**
     * Finds the single nearest point to (targetLat, targetLon).
     * If safeHavenOnly is true, only points with isSafeHaven == true are considered.
     */
    [[nodiscard]] std::optional<SpatialPoint> findNearest(
        double targetLat, double targetLon,
        bool safeHavenOnly = false
    ) const;

    /**
     * Finds the K nearest points to (targetLat, targetLon).
     */
    [[nodiscard]] std::vector<SpatialPoint> findKNearest(
        double targetLat, double targetLon,
        size_t k,
        bool safeHavenOnly = false
    ) const;

    /**
     * Finds all points within radiusMeters from (targetLat, targetLon).
     */
    [[nodiscard]] std::vector<SpatialPoint> findWithinRadius(
        double targetLat, double targetLon,
        double radiusMeters,
        bool safeHavenOnly = false
    ) const;

private:
    std::unique_ptr<KDNode> root_{nullptr};
    size_t size_{0};

    std::unique_ptr<KDNode> buildRecursive(
        std::vector<SpatialPoint>& pts,
        size_t start, size_t end,
        int depth
    );

    void nearestNeighborSearch(
        const KDNode* node,
        double targetLat, double targetLon,
        bool safeHavenOnly,
        const SpatialPoint*& bestPoint,
        double& bestDistMeters
    ) const;

    void radiusSearch(
        const KDNode* node,
        double targetLat, double targetLon,
        double radiusMeters,
        bool safeHavenOnly,
        std::vector<SpatialPoint>& results
    ) const;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_KDTREE_H
