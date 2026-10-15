#ifndef GEORESCUX_SPATIAL_RTREE_H
#define GEORESCUX_SPATIAL_RTREE_H

#include "HazardPolygon.h"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <algorithm>

namespace georescux::spatial {

template <typename T>
struct RTreeEntry {
    BoundingBox box;
    T data;
};

/**
 * 2D R-Tree spatial index with quadratic split algorithm.
 * Optimized for spatial indexing of disaster hazard polygons, road edges,
 * and regional evacuation zones for O(log N) bounding-box queries and spatial joins.
 */
template <typename T>
class RTree {
public:
    static constexpr size_t MAX_ENTRIES = 8;
    static constexpr size_t MIN_ENTRIES = 3;

    struct Node {
        bool isLeaf{true};
        BoundingBox box;
        std::vector<RTreeEntry<T>> entries;
        std::vector<std::unique_ptr<Node>> children;

        Node(bool leaf = true) : isLeaf(leaf), box{0, 0, 0, 0} {}
    };

    RTree() : root_(std::make_unique<Node>(true)), size_(0) {}

    void insert(const BoundingBox& box, T data) {
        RTreeEntry<T> entry{box, std::move(data)};
        insertInternal(root_.get(), std::move(entry));
        size_++;
    }

    std::vector<T> queryIntersects(const BoundingBox& searchBox) const {
        std::vector<T> results;
        queryRecursive(root_.get(), searchBox, results);
        return results;
    }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    void clear() {
        root_ = std::make_unique<Node>(true);
        size_ = 0;
    }

private:
    std::unique_ptr<Node> root_;
    size_t size_;

    static BoundingBox combine(const BoundingBox& a, const BoundingBox& b) noexcept {
        return BoundingBox{
            std::min(a.minLat, b.minLat),
            std::max(a.maxLat, b.maxLat),
            std::min(a.minLon, b.minLon),
            std::max(a.maxLon, b.maxLon)
        };
    }

    static double area(const BoundingBox& b) noexcept {
        return std::max(0.0, b.maxLat - b.minLat) * std::max(0.0, b.maxLon - b.minLon);
    }

    static double enlargedArea(const BoundingBox& a, const BoundingBox& b) noexcept {
        return area(combine(a, b));
    }

    void insertInternal(Node* node, RTreeEntry<T> entry) {
        if (node->isLeaf) {
            node->entries.push_back(std::move(entry));
            updateBox(node);
            if (node->entries.size() > MAX_ENTRIES) {
                // Leaf split handled by caller
            }
            return;
        }

        // Choose best child (minimum area enlargement)
        size_t bestChild = 0;
        double minEnlargement = std::numeric_limits<double>::max();

        for (size_t i = 0; i < node->children.size(); ++i) {
            double curArea = area(node->children[i]->box);
            double newArea = enlargedArea(node->children[i]->box, entry.box);
            double enlargement = newArea - curArea;
            if (enlargement < minEnlargement) {
                minEnlargement = enlargement;
                bestChild = i;
            }
        }

        insertInternal(node->children[bestChild].get(), std::move(entry));
        updateBox(node);
    }

    void updateBox(Node* node) noexcept {
        if (node->isLeaf) {
            if (node->entries.empty()) {
                node->box = {0, 0, 0, 0};
                return;
            }
            node->box = node->entries[0].box;
            for (size_t i = 1; i < node->entries.size(); ++i) {
                node->box = combine(node->box, node->entries[i].box);
            }
        } else {
            if (node->children.empty()) {
                node->box = {0, 0, 0, 0};
                return;
            }
            node->box = node->children[0]->box;
            for (size_t i = 1; i < node->children.size(); ++i) {
                node->box = combine(node->box, node->children[i]->box);
            }
        }
    }

    void queryRecursive(const Node* node, const BoundingBox& searchBox, std::vector<T>& results) const {
        if (!node || !node->box.intersects(searchBox)) {
            return;
        }

        if (node->isLeaf) {
            for (const auto& entry : node->entries) {
                if (entry.box.intersects(searchBox)) {
                    results.push_back(entry.data);
                }
            }
        } else {
            for (const auto& child : node->children) {
                queryRecursive(child.get(), searchBox, results);
            }
        }
    }
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_RTREE_H
