#ifndef GEORESCUX_SPATIAL_GEODESIC_POLYGON_H
#define GEORESCUX_SPATIAL_GEODESIC_POLYGON_H

#include "HazardPolygon.h"
#include <vector>

namespace georescux::spatial {

/**
 * Spherical and ellipsoidal geometry calculations for disaster exclusion zones.
 * Computes exact spherical area, geodesic centroid, convex hull, and safety buffer expansions.
 */
class GeodesicPolygon {
public:
    static constexpr double EARTH_RADIUS_METERS = 6371008.8;

    /**
     * Calculates the spherical surface area of a polygon on Earth in square meters
     * using spherical excess (l'Huilier's / Girard's theorem).
     */
    static double sphericalAreaSquareMeters(const std::vector<Coordinate2D>& vertices) noexcept;

    /**
     * Computes the geographical centroid of a polygon.
     */
    static Coordinate2D geodesicCentroid(const std::vector<Coordinate2D>& vertices) noexcept;

    /**
     * Computes the 2D convex hull of a cloud of distress coordinates
     * using Andrew's Monotone Chain algorithm (O(N log N)).
     */
    static std::vector<Coordinate2D> computeConvexHull(std::vector<Coordinate2D> points);

    /**
     * Expands polygon outward by bufferMeters to generate a mandatory evacuation perimeter.
     */
    static std::vector<Coordinate2D> bufferPolygon(
        const std::vector<Coordinate2D>& vertices,
        double bufferMeters
    );
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_GEODESIC_POLYGON_H
