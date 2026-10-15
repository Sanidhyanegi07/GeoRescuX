#ifndef GEORESCUX_SPATIAL_HAZARD_POLYGON_H
#define GEORESCUX_SPATIAL_HAZARD_POLYGON_H

#include <vector>
#include <string>
#include <utility>
#include <cmath>

namespace georescux::spatial {

struct Coordinate2D {
    double latitude;
    double longitude;
};
using Point2D = Coordinate2D;

struct BoundingBox {
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;

    [[nodiscard]] bool contains(double lat, double lon) const noexcept {
        return lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon;
    }

    [[nodiscard]] bool intersects(const BoundingBox& other) const noexcept {
        return !(maxLat < other.minLat || minLat > other.maxLat ||
                 maxLon < other.minLon || minLon > other.maxLon);
    }
};

enum class HazardSeverity {
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    CRITICAL_EVACUATE = 4
};

/**
 * Geometric polygon representing an active disaster hazard perimeter
 * (e.g. flash-flood boundary, active landslide zone, wildfire perimeter).
 */
class HazardPolygon {
public:
    HazardPolygon(
        std::string id,
        std::string hazardType,
        HazardSeverity severity,
        std::vector<Coordinate2D> vertices,
        double penaltyCostMeters = 5000.0
    );

    [[nodiscard]] const std::string& getId() const noexcept { return id_; }
    [[nodiscard]] const std::string& getHazardType() const noexcept { return hazardType_; }
    [[nodiscard]] HazardSeverity getSeverity() const noexcept { return severity_; }
    [[nodiscard]] double getPenaltyCostMeters() const noexcept { return penaltyCostMeters_; }
    [[nodiscard]] const std::vector<Coordinate2D>& getVertices() const noexcept { return vertices_; }
    [[nodiscard]] const BoundingBox& getBoundingBox() const noexcept { return bbox_; }

    /**
     * Determines whether (lat, lon) is inside the active hazard zone
     * using the Jordan Curve (Ray Casting) algorithm with bounding-box early pruning.
     */
    [[nodiscard]] bool containsPoint(double lat, double lon) const noexcept;

    /**
     * Checks if a straight-line road edge (startLat, startLon) -> (endLat, endLon)
     * intersects this hazard zone.
     */
    [[nodiscard]] bool intersectsEdge(
        double startLat, double startLon,
        double endLat, double endLon
    ) const noexcept;

    /**
     * Calculates minimum clearance distance (in meters) from a point to the hazard perimeter.
     * If point is inside, returns negative distance.
     */
    [[nodiscard]] double distanceToPerimeterMeters(double lat, double lon) const noexcept;

private:
    std::string id_;
    std::string hazardType_;
    HazardSeverity severity_;
    std::vector<Coordinate2D> vertices_;
    double penaltyCostMeters_;
    BoundingBox bbox_{};

    void computeBoundingBox() noexcept;

    static bool lineSegmentsIntersect(
        double p1x, double p1y, double q1x, double q1y,
        double p2x, double p2y, double q2x, double q2y
    ) noexcept;

    static double orientation(double px, double py, double qx, double qy, double rx, double ry) noexcept;
    static bool onSegment(double px, double py, double qx, double qy, double rx, double ry) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_HAZARD_POLYGON_H
