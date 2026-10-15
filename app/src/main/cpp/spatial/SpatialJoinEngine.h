#ifndef GEORESCUX_SPATIAL_JOIN_ENGINE_H
#define GEORESCUX_SPATIAL_JOIN_ENGINE_H

#include <vector>
#include <string>
#include <memory>
#include "HazardPolygon.h"

namespace georescux::spatial {

struct RoadSegment {
    int64_t segmentId;
    double startLat;
    double startLon;
    double endLat;
    double endLon;
    double lengthMeters;
};

struct SeveredRoadResult {
    int64_t segmentId;
    std::string hazardId;
    double breachLat;
    double breachLon;
    double penetrationDepthMeters;
};

struct Polyline {
    std::vector<Point2D> points;
};

/**
 * High-performance GIS Spatial Join and Clipping Engine for Disaster Zones.
 * Intersects road networks against dynamic hazard perimeters,
 * performs polygon clipping, and computes safety buffer zones.
 */
class SpatialJoinEngine {
public:
    SpatialJoinEngine() = default;

    /**
     * Intersects a collection of road segments against active hazard perimeters.
     * Returns list of severed roads with exact breach locations.
     */
    static std::vector<SeveredRoadResult> findSeveredRoads(
        const std::vector<RoadSegment>& roads,
        const std::vector<HazardPolygon>& hazards
    );

    /**
     * Clips a route polyline against a safe region or convex polygon
     * using the Sutherland-Hodgman polygon clipping algorithm.
     */
    static Polyline clipPolylineAgainstPolygon(
        const Polyline& subjectPolyline,
        const std::vector<Point2D>& clipPolygonVertices
    );

    /**
     * Generates an outward buffer polygon of bufferRadiusMeters around an arbitrary polygon
     * using vertex normal offset and intersection resolution.
     */
    static std::vector<Point2D> generateBufferedPolygon(
        const std::vector<Point2D>& inputPolygon,
        double bufferRadiusMeters
    );

    /**
     * Finds the intersection point between two line segments (p1-p2) and (p3-p4).
     * Returns true if they intersect, with the intersection point in outIntersection.
     */
    static bool findSegmentIntersection(
        const Point2D& p1, const Point2D& p2,
        const Point2D& p3, const Point2D& p4,
        Point2D& outIntersection
    ) noexcept;

    /**
     * Calculates minimum distance in meters from a point to a road segment.
     */
    static double distancePointToSegmentMeters(
        const Point2D& point,
        const Point2D& segStart,
        const Point2D& segEnd
    ) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_JOIN_ENGINE_H
