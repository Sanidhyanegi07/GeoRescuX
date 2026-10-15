#ifndef GEORESCUX_SPATIAL_GEODESIC_MATH_H
#define GEORESCUX_SPATIAL_GEODESIC_MATH_H

#include <cmath>
#include <numbers>
#include <utility>

namespace georescux::spatial {

/**
 * High-performance geodesic mathematics engine for disaster mapping and evacuation.
 * Implements high-precision Haversine, Vincenty geodesic distance, initial bearing,
 * cross-track distance, and destination point projections.
 */
class GeodesicMath {
public:
    static constexpr double EARTH_RADIUS_METERS = 6371008.8; // IUGG mean Earth radius
    static constexpr double WGS84_A = 6378137.0;            // Semi-major axis (meters)
    static constexpr double WGS84_B = 6356752.314245;       // Semi-minor axis (meters)
    static constexpr double WGS84_F = 1.0 / 298.257223563;  // Flattening

    /**
     * Converts degrees to radians.
     */
    static inline double toRadians(double degrees) noexcept {
        return degrees * (std::numbers::pi / 180.0);
    }

    /**
     * Converts radians to degrees.
     */
    static inline double toDegrees(double radians) noexcept {
        return radians * (180.0 / std::numbers::pi);
    }

    /**
     * Fast Haversine great-circle distance between two (lat, lon) coordinates in meters.
     */
    static double haversineDistanceMeters(
        double lat1, double lon1,
        double lat2, double lon2
    ) noexcept;

    /**
     * High-accuracy Vincenty inverse formula on the WGS-84 ellipsoid.
     * Accurate to ~0.5 mm over the entire ellipsoid.
     */
    static double vincentyDistanceMeters(
        double lat1, double lon1,
        double lat2, double lon2,
        int maxIterations = 200
    ) noexcept;

    /**
     * Initial compass bearing from (lat1, lon1) to (lat2, lon2) in degrees [0, 360).
     */
    static double initialBearingDegrees(
        double lat1, double lon1,
        double lat2, double lon2
    ) noexcept;

    /**
     * Computes the destination point given start coordinate, distance in meters, and bearing in degrees.
     * Returns pair of {latitude, longitude} in degrees.
     */
    static std::pair<double, double> destinationPoint(
        double lat, double lon,
        double distanceMeters,
        double bearingDegrees
    ) noexcept;

    /**
     * Calculates the cross-track distance of a point from a geodesic path in meters.
     * Useful for checking if an evacuation traveler is deviating from their safe route.
     */
    static double crossTrackDistanceMeters(
        double pathStartLat, double pathStartLon,
        double pathEndLat, double pathEndLon,
        double pointLat, double pointLon
    ) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_GEODESIC_MATH_H
