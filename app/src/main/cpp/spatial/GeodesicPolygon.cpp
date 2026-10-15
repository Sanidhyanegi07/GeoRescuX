#include "GeodesicPolygon.h"
#include "GeodesicMath.h"
#include <algorithm>
#include <cmath>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
inline double toRad(double deg) { return deg * (PI / 180.0); }
inline double toDeg(double rad) { return rad * (180.0 / PI); }

double crossProduct(const Coordinate2D& o, const Coordinate2D& a, const Coordinate2D& b) noexcept {
    return (a.longitude - o.longitude) * (b.latitude - o.latitude) -
           (a.latitude - o.latitude) * (b.longitude - o.longitude);
}
} // namespace

double GeodesicPolygon::sphericalAreaSquareMeters(const std::vector<Coordinate2D>& vertices) noexcept {
    if (vertices.size() < 3) return 0.0;

    double totalExcess = 0.0;
    const size_t n = vertices.size();

    for (size_t i = 0; i < n; ++i) {
        const auto& p1 = vertices[i];
        const auto& p2 = vertices[(i + 1) % n];

        const double lambda1 = toRad(p1.longitude);
        const double phi1 = toRad(p1.latitude);
        const double lambda2 = toRad(p2.longitude);
        const double phi2 = toRad(p2.latitude);

        totalExcess += (lambda2 - lambda1) * (2.0 + std::sin(phi1) + std::sin(phi2));
    }

    const double area = std::abs(totalExcess * EARTH_RADIUS_METERS * EARTH_RADIUS_METERS * 0.5);
    return area;
}

Coordinate2D GeodesicPolygon::geodesicCentroid(const std::vector<Coordinate2D>& vertices) noexcept {
    if (vertices.empty()) return Coordinate2D{0.0, 0.0};
    if (vertices.size() == 1) return vertices[0];

    // Compute average 3D Cartesian coordinates on unit sphere to avoid meridian/pole wrap issues
    double x = 0.0, y = 0.0, z = 0.0;

    for (const auto& v : vertices) {
        const double latRad = toRad(v.latitude);
        const double lonRad = toRad(v.longitude);

        x += std::cos(latRad) * std::cos(lonRad);
        y += std::cos(latRad) * std::sin(lonRad);
        z += std::sin(latRad);
    }

    const double total = static_cast<double>(vertices.size());
    x /= total;
    y /= total;
    z /= total;

    const double centralLon = std::atan2(y, x);
    const double centralHyp = std::sqrt(x * x + y * y);
    const double centralLat = std::atan2(z, centralHyp);

    return Coordinate2D{
        .latitude = toDeg(centralLat),
        .longitude = toDeg(centralLon)
    };
}

std::vector<Coordinate2D> GeodesicPolygon::computeConvexHull(std::vector<Coordinate2D> points) {
    if (points.size() <= 2) return points;

    // Sort lexicographically by longitude, then latitude
    std::sort(points.begin(), points.end(), [](const Coordinate2D& a, const Coordinate2D& b) {
        if (std::abs(a.longitude - b.longitude) > 1e-9) {
            return a.longitude < b.longitude;
        }
        return a.latitude < b.latitude;
    });

    std::vector<Coordinate2D> hull;
    hull.reserve(points.size() * 2);

    // Lower hull
    for (const auto& pt : points) {
        while (hull.size() >= 2 && crossProduct(hull[hull.size() - 2], hull.back(), pt) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(pt);
    }

    // Upper hull
    const size_t lowerSize = hull.size();
    for (int i = static_cast<int>(points.size()) - 2; i >= 0; --i) {
        while (hull.size() > lowerSize && crossProduct(hull[hull.size() - 2], hull.back(), points[i]) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(points[i]);
    }

    hull.pop_back(); // Remove duplicate point
    return hull;
}

std::vector<Coordinate2D> GeodesicPolygon::bufferPolygon(
    const std::vector<Coordinate2D>& vertices,
    double bufferMeters
) {
    if (vertices.empty() || bufferMeters <= 0.0) return vertices;

    const auto center = geodesicCentroid(vertices);
    std::vector<Coordinate2D> buffered;
    buffered.reserve(vertices.size());

    for (const auto& v : vertices) {
        double bearing = GeodesicMath::initialBearingDegrees(center.latitude, center.longitude, v.latitude, v.longitude);
        double dist = GeodesicMath::haversineDistanceMeters(center.latitude, center.longitude, v.latitude, v.longitude);
        auto expanded = GeodesicMath::destinationPoint(center.latitude, center.longitude, dist + bufferMeters, bearing);
        buffered.push_back(Coordinate2D{expanded.first, expanded.second});
    }

    return buffered;
}

} // namespace georescux::spatial
