#include "HazardPolygon.h"
#include "GeodesicMath.h"
#include <algorithm>
#include <limits>

namespace georescux::spatial {

HazardPolygon::HazardPolygon(
    std::string id,
    std::string hazardType,
    HazardSeverity severity,
    std::vector<Coordinate2D> vertices,
    double penaltyCostMeters
) : id_(std::move(id)),
    hazardType_(std::move(hazardType)),
    severity_(severity),
    vertices_(std::move(vertices)),
    penaltyCostMeters_(penaltyCostMeters) {
    computeBoundingBox();
}

void HazardPolygon::computeBoundingBox() noexcept {
    if (vertices_.empty()) {
        bbox_ = {0.0, 0.0, 0.0, 0.0};
        return;
    }
    double minLat = std::numeric_limits<double>::max();
    double maxLat = std::numeric_limits<double>::lowest();
    double minLon = std::numeric_limits<double>::max();
    double maxLon = std::numeric_limits<double>::lowest();

    for (const auto& v : vertices_) {
        minLat = std::min(minLat, v.latitude);
        maxLat = std::max(maxLat, v.latitude);
        minLon = std::min(minLon, v.longitude);
        maxLon = std::max(maxLon, v.longitude);
    }
    bbox_ = {minLat, maxLat, minLon, maxLon};
}

bool HazardPolygon::containsPoint(double lat, double lon) const noexcept {
    if (vertices_.size() < 3) return false;

    // Fast bounding box rejection test
    if (!bbox_.contains(lat, lon)) return false;

    bool inside = false;
    const size_t n = vertices_.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = vertices_[i].longitude;
        const double yi = vertices_[i].latitude;
        const double xj = vertices_[j].longitude;
        const double yj = vertices_[j].latitude;

        const bool intersect = ((yi > lat) != (yj > lat)) &&
            (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi);

        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

double HazardPolygon::orientation(double px, double py, double qx, double qy, double rx, double ry) noexcept {
    const double val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    if (std::abs(val) < 1e-12) return 0.0; // collinear
    return (val > 0.0) ? 1.0 : 2.0;       // clockwise or counterclockwise
}

bool HazardPolygon::onSegment(double px, double py, double qx, double qy, double rx, double ry) noexcept {
    return (qx <= std::max(px, rx) && qx >= std::min(px, rx) &&
            qy <= std::max(py, ry) && qy >= std::min(py, ry));
}

bool HazardPolygon::lineSegmentsIntersect(
    double p1x, double p1y, double q1x, double q1y,
    double p2x, double p2y, double q2x, double q2y
) noexcept {
    const double o1 = orientation(p1x, p1y, q1x, q1y, p2x, p2y);
    const double o2 = orientation(p1x, p1y, q1x, q1y, q2x, q2y);
    const double o3 = orientation(p2x, p2y, q2x, q2y, p1x, p1y);
    const double o4 = orientation(p2x, p2y, q2x, q2y, q1x, q1y);

    if (o1 != o2 && o3 != o4) return true;

    if (o1 == 0.0 && onSegment(p1x, p1y, p2x, p2y, q1x, q1y)) return true;
    if (o2 == 0.0 && onSegment(p1x, p1y, q2x, q2y, q1x, q1y)) return true;
    if (o3 == 0.0 && onSegment(p2x, p2y, p1x, p1y, q2x, q2y)) return true;
    if (o4 == 0.0 && onSegment(p2x, p2y, q1x, q1y, q2x, q2y)) return true;

    return false;
}

bool HazardPolygon::intersectsEdge(
    double startLat, double startLon,
    double endLat, double endLon
) const noexcept {
    if (vertices_.size() < 3) return false;

    // Check if either endpoint is inside the polygon
    if (containsPoint(startLat, startLon) || containsPoint(endLat, endLon)) {
        return true;
    }

    const BoundingBox edgeBox{
        std::min(startLat, endLat),
        std::max(startLat, endLat),
        std::min(startLon, endLon),
        std::max(startLon, endLon)
    };

    if (!bbox_.intersects(edgeBox)) {
        return false;
    }

    const size_t n = vertices_.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (lineSegmentsIntersect(
            startLon, startLat, endLon, endLat,
            vertices_[j].longitude, vertices_[j].latitude,
            vertices_[i].longitude, vertices_[i].latitude
        )) {
            return true;
        }
    }

    return false;
}

double HazardPolygon::distanceToPerimeterMeters(double lat, double lon) const noexcept {
    if (vertices_.empty()) return std::numeric_limits<double>::max();

    double minDistance = std::numeric_limits<double>::max();
    const size_t n = vertices_.size();

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double d = GeodesicMath::crossTrackDistanceMeters(
            vertices_[j].latitude, vertices_[j].longitude,
            vertices_[i].latitude, vertices_[i].longitude,
            lat, lon
        );
        minDistance = std::min(minDistance, std::abs(d));
    }

    const bool inside = containsPoint(lat, lon);
    return inside ? -minDistance : minDistance;
}

} // namespace georescux::spatial
