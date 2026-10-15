#include "SpatialJoinEngine.h"
#include <algorithm>
#include <cmath>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_M = 6371008.8;

double crossProduct2D(const Point2D& a, const Point2D& b) noexcept {
    return a.longitude * b.latitude - a.latitude * b.longitude;
}

Point2D subtractPoints(const Point2D& a, const Point2D& b) noexcept {
    return {a.latitude - b.latitude, a.longitude - b.longitude};
}

Point2D addPoints(const Point2D& a, const Point2D& b) noexcept {
    return {a.latitude + b.latitude, a.longitude + b.longitude};
}

double haversineMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_M * c;
}
} // namespace

bool SpatialJoinEngine::findSegmentIntersection(
    const Point2D& p1, const Point2D& p2,
    const Point2D& p3, const Point2D& p4,
    Point2D& outIntersection
) noexcept {
    Point2D r = subtractPoints(p2, p1);
    Point2D s = subtractPoints(p4, p3);
    Point2D qp = subtractPoints(p3, p1);

    double rCrossS = crossProduct2D(r, s);
    double qpCrossR = crossProduct2D(qp, r);

    // Parallel or collinear
    if (std::abs(rCrossS) < 1e-12) {
        return false;
    }

    double t = crossProduct2D(qp, s) / rCrossS;
    double u = qpCrossR / rCrossS;

    if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
        outIntersection.latitude = p1.latitude + t * r.latitude;
        outIntersection.longitude = p1.longitude + t * r.longitude;
        return true;
    }

    return false;
}

double SpatialJoinEngine::distancePointToSegmentMeters(
    const Point2D& p,
    const Point2D& a,
    const Point2D& b
) noexcept {
    double segLen2 = (b.latitude - a.latitude) * (b.latitude - a.latitude) +
                     (b.longitude - a.longitude) * (b.longitude - a.longitude);
    if (segLen2 < 1e-12) {
        return haversineMeters(p.latitude, p.longitude, a.latitude, a.longitude);
    }

    // Projection factor t
    double t = ((p.latitude - a.latitude) * (b.latitude - a.latitude) +
                (p.longitude - a.longitude) * (b.longitude - a.longitude)) / segLen2;
    t = std::clamp(t, 0.0, 1.0);

    Point2D proj = {
        a.latitude + t * (b.latitude - a.latitude),
        a.longitude + t * (b.longitude - a.longitude)
    };

    return haversineMeters(p.latitude, p.longitude, proj.latitude, proj.longitude);
}

std::vector<SeveredRoadResult> SpatialJoinEngine::findSeveredRoads(
    const std::vector<RoadSegment>& roads,
    const std::vector<HazardPolygon>& hazards
) {
    std::vector<SeveredRoadResult> results;

    for (const auto& road : roads) {
        Point2D rStart = {road.startLat, road.startLon};
        Point2D rEnd = {road.endLat, road.endLon};

        for (const auto& hazard : hazards) {
            bool startInside = hazard.containsPoint(rStart.latitude, rStart.longitude);
            bool endInside = hazard.containsPoint(rEnd.latitude, rEnd.longitude);

            if (startInside || endInside) {
                // Road penetrates or starts within the hazard
                Point2D breach = startInside ? rStart : rEnd;
                double penDepth = startInside && endInside ? road.lengthMeters : road.lengthMeters * 0.5;
                results.push_back({road.segmentId, hazard.getId(), breach.latitude, breach.longitude, penDepth});
                break; // already severed by this hazard
            }

            // Check if road intersects any of the hazard edges
            const auto& vertices = hazard.getVertices();
            if (vertices.size() < 3) continue;

            bool intersected = false;
            for (size_t i = 0; i < vertices.size(); ++i) {
                const auto& h1 = vertices[i];
                const auto& h2 = vertices[(i + 1) % vertices.size()];

                Point2D intersectionPt{0.0, 0.0};
                if (findSegmentIntersection(rStart, rEnd, h1, h2, intersectionPt)) {
                    double distToStart = haversineMeters(rStart.latitude, rStart.longitude, intersectionPt.latitude, intersectionPt.longitude);
                    results.push_back({road.segmentId, hazard.getId(), intersectionPt.latitude, intersectionPt.longitude, distToStart});
                    intersected = true;
                    break;
                }
            }

            if (intersected) break;
        }
    }

    return results;
}

Polyline SpatialJoinEngine::clipPolylineAgainstPolygon(
    const Polyline& subjectPolyline,
    const std::vector<Point2D>& clipPolygonVertices
) {
    if (subjectPolyline.points.size() < 2 || clipPolygonVertices.size() < 3) {
        return subjectPolyline;
    }

    std::vector<Point2D> outputList = subjectPolyline.points;

    for (size_t i = 0; i < clipPolygonVertices.size(); ++i) {
        const auto& edgeStart = clipPolygonVertices[i];
        const auto& edgeEnd = clipPolygonVertices[(i + 1) % clipPolygonVertices.size()];

        std::vector<Point2D> inputList = outputList;
        outputList.clear();

        if (inputList.empty()) break;

        // Inside half-plane test (left of edge vector)
        auto isInside = [&](const Point2D& pt) -> bool {
            return (edgeEnd.longitude - edgeStart.longitude) * (pt.latitude - edgeStart.latitude) -
                   (edgeEnd.latitude - edgeStart.latitude) * (pt.longitude - edgeStart.longitude) >= 0.0;
        };

        for (size_t j = 0; j < inputList.size() - 1; ++j) {
            const auto& s = inputList[j];
            const auto& e = inputList[j + 1];

            bool sIn = isInside(s);
            bool eIn = isInside(e);

            if (sIn && eIn) {
                if (outputList.empty() || (outputList.back().latitude != s.latitude || outputList.back().longitude != s.longitude)) {
                    outputList.push_back(s);
                }
                outputList.push_back(e);
            } else if (sIn && !eIn) {
                if (outputList.empty() || (outputList.back().latitude != s.latitude || outputList.back().longitude != s.longitude)) {
                    outputList.push_back(s);
                }
                Point2D inter{0.0, 0.0};
                if (findSegmentIntersection(s, e, edgeStart, edgeEnd, inter)) {
                    outputList.push_back(inter);
                }
            } else if (!sIn && eIn) {
                Point2D inter{0.0, 0.0};
                if (findSegmentIntersection(s, e, edgeStart, edgeEnd, inter)) {
                    outputList.push_back(inter);
                }
                outputList.push_back(e);
            }
        }
    }

    return Polyline{outputList};
}

std::vector<Point2D> SpatialJoinEngine::generateBufferedPolygon(
    const std::vector<Point2D>& inputPolygon,
    double bufferRadiusMeters
) {
    if (inputPolygon.size() < 3) return inputPolygon;

    std::vector<Point2D> buffered;
    buffered.reserve(inputPolygon.size());

    // Approximate meters to degrees at local latitude
    double midLat = 0.0;
    for (const auto& p : inputPolygon) midLat += p.latitude;
    midLat /= inputPolygon.size();

    double latDegPerMeter = 1.0 / 111139.0;
    double lonDegPerMeter = 1.0 / (111139.0 * std::cos(midLat * DEG2RAD));

    double deltaLat = bufferRadiusMeters * latDegPerMeter;
    double deltaLon = bufferRadiusMeters * lonDegPerMeter;

    size_t n = inputPolygon.size();
    for (size_t i = 0; i < n; ++i) {
        const auto& prev = inputPolygon[(i + n - 1) % n];
        const auto& curr = inputPolygon[i];
        const auto& next = inputPolygon[(i + 1) % n];

        // Edge 1 vector
        double v1Lat = curr.latitude - prev.latitude;
        double v1Lon = curr.longitude - prev.longitude;
        double len1 = std::sqrt(v1Lat * v1Lat + v1Lon * v1Lon);
        if (len1 > 1e-9) { v1Lat /= len1; v1Lon /= len1; }

        // Edge 2 vector
        double v2Lat = next.latitude - curr.latitude;
        double v2Lon = next.longitude - curr.longitude;
        double len2 = std::sqrt(v2Lat * v2Lat + v2Lon * v2Lon);
        if (len2 > 1e-9) { v2Lat /= len2; v2Lon /= len2; }

        // Outward normals: (-dy, dx)
        double n1Lat = -v1Lon;
        double n1Lon = v1Lat;
        double n2Lat = -v2Lon;
        double n2Lon = v2Lat;

        // Average normal
        double avgNLat = (n1Lat + n2Lat) * 0.5;
        double avgNLon = (n1Lon + n2Lon) * 0.5;
        double avgLen = std::sqrt(avgNLat * avgNLat + avgNLon * avgNLon);
        if (avgLen > 1e-9) {
            avgNLat /= avgLen;
            avgNLon /= avgLen;
        }

        // Clamp miter to avoid sharp spikes
        double miterFactor = 1.0;
        double dot = v1Lat * v2Lat + v1Lon * v2Lon;
        if (dot > -0.99) {
            miterFactor = std::min(2.5, 1.0 / std::max(0.2, std::sqrt((1.0 + dot) * 0.5)));
        }

        Point2D offsetPt{
            curr.latitude + avgNLat * deltaLat * miterFactor,
            curr.longitude + avgNLon * deltaLon * miterFactor
        };
        buffered.push_back(offsetPt);
    }

    return buffered;
}

} // namespace georescux::spatial
