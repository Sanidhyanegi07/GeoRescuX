#ifndef GEORESCUX_VORONOI_TESSELLATION_H
#define GEORESCUX_VORONOI_TESSELLATION_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "HazardPolygon.h"

namespace georescux::spatial {

struct ReliefSite {
    int64_t siteId;
    std::string name;
    double latitude;
    double longitude;
    int capacity;
    bool hasMedicalPost;
};

struct VoronoiEdge {
    Point2D start;
    Point2D end;
    int64_t leftSiteId;
    int64_t rightSiteId;
};

struct VoronoiCell {
    int64_t siteId;
    Point2D siteLocation;
    std::vector<Point2D> polygonVertices;
    double areaSqKm;
};

struct DelaunayTriangle {
    int64_t siteIdA;
    int64_t siteIdB;
    int64_t siteIdC;
    Point2D circumcenter;
    double circumradius;
};

/**
 * 2D Voronoi Tessellation and Delaunay Triangulation Engine.
 * Partitions disaster territory into relief catchment zones,
 * generates emergency wireless mesh backbone topologies,
 * and performs fast nearest-shelter spatial point location.
 */
class VoronoiTessellation {
public:
    VoronoiTessellation() = default;

    /**
     * Constructs Voronoi diagram and Delaunay triangulation from relief shelter sites.
     * Bounds coordinates clip infinite Voronoi rays to the regional disaster boundary.
     */
    void buildTessellation(
        const std::vector<ReliefSite>& sites,
        double minLat, double maxLat,
        double minLon, double maxLon
    );

    /**
     * Finds the Voronoi catchment cell enclosing a victim's GPS location.
     */
    [[nodiscard]] int64_t findEnclosingSiteId(double lat, double lon) const noexcept;

    /**
     * Gets the generated Voronoi cell for a specific relief site.
     */
    [[nodiscard]] const VoronoiCell* getCellForSite(int64_t siteId) const noexcept;

    /**
     * Returns all generated Voronoi cells.
     */
    [[nodiscard]] const std::vector<VoronoiCell>& getAllCells() const noexcept { return cells_; }

    /**
     * Returns the Delaunay Triangulation edges representing the emergency mesh radio backbone.
     */
    [[nodiscard]] std::vector<std::pair<int64_t, int64_t>> getDelaunayMeshBackbone() const;

    /**
     * Returns all Delaunay triangles.
     */
    [[nodiscard]] const std::vector<DelaunayTriangle>& getDelaunayTriangles() const noexcept { return triangles_; }

private:
    std::vector<ReliefSite> sites_;
    std::vector<VoronoiCell> cells_;
    std::vector<DelaunayTriangle> triangles_;
    double minLat_ = 0.0, maxLat_ = 0.0, minLon_ = 0.0, maxLon_ = 0.0;

    void computeBowyerWatsonTriangulation();
    void generateVoronoiFromDual();
    static bool isPointInCircumcircle(
        const Point2D& pt,
        const Point2D& a, const Point2D& b, const Point2D& c
    ) noexcept;
    static Point2D computeCircumcenter(
        const Point2D& a, const Point2D& b, const Point2D& c,
        double& outRadius
    ) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_VORONOI_TESSELLATION_H
