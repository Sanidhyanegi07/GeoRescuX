#include "VoronoiTessellation.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <map>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double EARTH_RADIUS_KM = 6371.0088;

double distSq(const Point2D& p1, const Point2D& p2) noexcept {
    double dLat = p1.latitude - p2.latitude;
    double dLon = p1.longitude - p2.longitude;
    return dLat * dLat + dLon * dLon;
}

double haversineDistanceKm(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_KM * c;
}
} // namespace

Point2D VoronoiTessellation::computeCircumcenter(
    const Point2D& a, const Point2D& b, const Point2D& c,
    double& outRadius
) noexcept {
    double d = 2.0 * (a.latitude * (b.longitude - c.longitude) +
                      b.latitude * (c.longitude - a.longitude) +
                      c.latitude * (a.longitude - b.longitude));

    if (std::abs(d) < 1e-11) {
        outRadius = 1e9;
        return {(a.latitude + b.latitude + c.latitude) / 3.0, (a.longitude + b.longitude + c.longitude) / 3.0};
    }

    double a2 = a.latitude * a.latitude + a.longitude * a.longitude;
    double b2 = b.latitude * b.latitude + b.longitude * b.longitude;
    double c2 = c.latitude * c.latitude + c.longitude * c.longitude;

    double ux = (a2 * (b.longitude - c.longitude) +
                 b2 * (c.longitude - a.longitude) +
                 c2 * (a.longitude - b.longitude)) / d;

    double uy = (a2 * (c.latitude - b.latitude) +
                 b2 * (a.latitude - c.latitude) +
                 c2 * (b.latitude - a.latitude)) / d;

    Point2D center{ux, uy};
    outRadius = std::sqrt((ux - a.latitude) * (ux - a.latitude) + (uy - a.longitude) * (uy - a.longitude));
    return center;
}

bool VoronoiTessellation::isPointInCircumcircle(
    const Point2D& pt,
    const Point2D& a, const Point2D& b, const Point2D& c
) noexcept {
    double r;
    Point2D center = computeCircumcenter(a, b, c, r);
    double d = std::sqrt(distSq(pt, center));
    return d <= (r + 1e-9);
}

void VoronoiTessellation::computeBowyerWatsonTriangulation() {
    triangles_.clear();
    if (sites_.size() < 3) return;

    // Create super-triangle enclosing the region
    double dLat = maxLat_ - minLat_;
    double dLon = maxLon_ - minLon_;
    double midLat = (minLat_ + maxLat_) * 0.5;
    double midLon = (minLon_ + maxLon_) * 0.5;

    // Virtual super-triangle vertices
    Point2D st1{midLat - 20.0 * dLat, midLon - 20.0 * dLon};
    Point2D st2{midLat - 20.0 * dLat, midLon + 20.0 * dLon};
    Point2D st3{midLat + 20.0 * dLat, midLon};

    int64_t ST_ID1 = -1001;
    int64_t ST_ID2 = -1002;
    int64_t ST_ID3 = -1003;

    double rad;
    Point2D cc = computeCircumcenter(st1, st2, st3, rad);
    triangles_.push_back({ST_ID1, ST_ID2, ST_ID3, cc, rad});

    // Point map for lookup
    std::map<int64_t, Point2D> pointMap;
    pointMap[ST_ID1] = st1;
    pointMap[ST_ID2] = st2;
    pointMap[ST_ID3] = st3;
    for (const auto& s : sites_) {
        pointMap[s.siteId] = {s.latitude, s.longitude};
    }

    struct Edge {
        int64_t u, v;
        bool operator<(const Edge& o) const {
            if (u != o.u) return u < o.u;
            return v < o.v;
        }
        bool operator==(const Edge& o) const {
            return (u == o.u && v == o.v) || (u == o.v && v == o.u);
        }
    };

    // Incremental Bowyer-Watson insertion
    for (const auto& site : sites_) {
        Point2D pt{site.latitude, site.longitude};
        std::vector<DelaunayTriangle> badTriangles;

        for (const auto& tri : triangles_) {
            if (isPointInCircumcircle(pt, pointMap[tri.siteIdA], pointMap[tri.siteIdB], pointMap[tri.siteIdC])) {
                badTriangles.push_back(tri);
            }
        }

        // Find boundary edges of the cavity
        std::map<Edge, int> edgeCount;
        for (const auto& tri : badTriangles) {
            auto addE = [&](int64_t a, int64_t b) {
                Edge e{std::min(a, b), std::max(a, b)};
                edgeCount[e]++;
            };
            addE(tri.siteIdA, tri.siteIdB);
            addE(tri.siteIdB, tri.siteIdC);
            addE(tri.siteIdC, tri.siteIdA);
        }

        // Remove bad triangles
        triangles_.erase(std::remove_if(triangles_.begin(), triangles_.end(), [&](const DelaunayTriangle& t) {
            for (const auto& bt : badTriangles) {
                if (t.siteIdA == bt.siteIdA && t.siteIdB == bt.siteIdB && t.siteIdC == bt.siteIdC) {
                    return true;
                }
            }
            return false;
        }), triangles_.end());

        // Re-triangulate cavity
        for (const auto& [e, count] : edgeCount) {
            if (count == 1) { // boundary edge
                double r;
                Point2D cCenter = computeCircumcenter(pointMap[e.u], pointMap[e.v], pt, r);
                triangles_.push_back({e.u, e.v, site.siteId, cCenter, r});
            }
        }
    }

    // Filter out triangles containing super-triangle vertices
    triangles_.erase(std::remove_if(triangles_.begin(), triangles_.end(), [&](const DelaunayTriangle& t) {
        return t.siteIdA < 0 || t.siteIdB < 0 || t.siteIdC < 0;
    }), triangles_.end());
}

void VoronoiTessellation::generateVoronoiFromDual() {
    cells_.clear();
    if (sites_.empty()) return;

    // Map each site to all its adjacent triangle circumcenters
    std::map<int64_t, std::vector<Point2D>> siteToCircumcenters;

    for (const auto& tri : triangles_) {
        siteToCircumcenters[tri.siteIdA].push_back(tri.circumcenter);
        siteToCircumcenters[tri.siteIdB].push_back(tri.circumcenter);
        siteToCircumcenters[tri.siteIdC].push_back(tri.circumcenter);
    }

    for (const auto& site : sites_) {
        VoronoiCell cell;
        cell.siteId = site.siteId;
        cell.siteLocation = {site.latitude, site.longitude};
        cell.areaSqKm = 0.0;

        auto it = siteToCircumcenters.find(site.siteId);
        if (it != siteToCircumcenters.end() && it->second.size() >= 3) {
            auto verts = it->second;

            // Sort vertices radially around site location
            std::sort(verts.begin(), verts.end(), [&](const Point2D& a, const Point2D& b) {
                double angleA = std::atan2(a.latitude - site.latitude, a.longitude - site.longitude);
                double angleB = std::atan2(b.latitude - site.latitude, b.longitude - site.longitude);
                return angleA < angleB;
            });

            // Clamp / clip vertices to bounding box
            for (auto& v : verts) {
                v.latitude = std::clamp(v.latitude, minLat_, maxLat_);
                v.longitude = std::clamp(v.longitude, minLon_, maxLon_);
            }

            // Approximate area using spherical polygon formula / Shoelace
            double area = 0.0;
            size_t n = verts.size();
            for (size_t i = 0; i < n; ++i) {
                size_t j = (i + 1) % n;
                double x1 = haversineDistanceKm(site.latitude, site.longitude, site.latitude, verts[i].longitude);
                double y1 = haversineDistanceKm(site.latitude, site.longitude, verts[i].latitude, site.longitude);
                double x2 = haversineDistanceKm(site.latitude, site.longitude, site.latitude, verts[j].longitude);
                double y2 = haversineDistanceKm(site.latitude, site.longitude, verts[j].latitude, site.longitude);
                if (verts[i].longitude < site.longitude) x1 = -x1;
                if (verts[i].latitude < site.latitude) y1 = -y1;
                if (verts[j].longitude < site.longitude) x2 = -x2;
                if (verts[j].latitude < site.latitude) y2 = -y2;
                area += (x1 * y2 - x2 * y1);
            }
            cell.areaSqKm = std::abs(area) * 0.5;
            cell.polygonVertices = std::move(verts);
        }

        cells_.push_back(std::move(cell));
    }
}

void VoronoiTessellation::buildTessellation(
    const std::vector<ReliefSite>& sites,
    double minLat, double maxLat,
    double minLon, double maxLon
) {
    sites_ = sites;
    minLat_ = minLat;
    maxLat_ = maxLat;
    minLon_ = minLon;
    maxLon_ = maxLon;

    computeBowyerWatsonTriangulation();
    generateVoronoiFromDual();
}

int64_t VoronoiTessellation::findEnclosingSiteId(double lat, double lon) const noexcept {
    if (sites_.empty()) return -1;

    // Nearest-site search (which is mathematical definition of Voronoi cell)
    int64_t nearestId = -1;
    double minDistance = std::numeric_limits<double>::infinity();

    for (const auto& site : sites_) {
        double d = haversineDistanceKm(lat, lon, site.latitude, site.longitude);
        if (d < minDistance) {
            minDistance = d;
            nearestId = site.siteId;
        }
    }

    return nearestId;
}

const VoronoiCell* VoronoiTessellation::getCellForSite(int64_t siteId) const noexcept {
    for (const auto& cell : cells_) {
        if (cell.siteId == siteId) {
            return &cell;
        }
    }
    return nullptr;
}

std::vector<std::pair<int64_t, int64_t>> VoronoiTessellation::getDelaunayMeshBackbone() const {
    std::set<std::pair<int64_t, int64_t>> uniqueEdges;

    for (const auto& tri : triangles_) {
        auto addE = [&](int64_t u, int64_t v) {
            uniqueEdges.insert({std::min(u, v), std::max(u, v)});
        };
        addE(tri.siteIdA, tri.siteIdB);
        addE(tri.siteIdB, tri.siteIdC);
        addE(tri.siteIdC, tri.siteIdA);
    }

    return std::vector<std::pair<int64_t, int64_t>>(uniqueEdges.begin(), uniqueEdges.end());
}

} // namespace georescux::spatial
