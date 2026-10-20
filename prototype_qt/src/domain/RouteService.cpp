#include "RouteService.h"
#include <cmath>
#include <limits>
#include <QVariantMap>
#include <QDebug>

namespace georescux::domain {

namespace {

constexpr double EARTH_RADIUS_M = 6371000.0;
constexpr double PI_VAL = 3.14159265358979323846;

inline double toRad(double deg) {
    return deg * (PI_VAL / 180.0);
}

double haversineDist(double lat1, double lon1, double lat2, double lon2) {
    const double dLat = toRad(lat2 - lat1);
    const double dLon = toRad(lon2 - lon1);
    const double h = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                     std::cos(toRad(lat1)) * std::cos(toRad(lat2)) *
                     std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    return 2.0 * EARTH_RADIUS_M * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
}

bool isValidCoordinate(double lat, double lon) {
    if (std::isnan(lat) || std::isnan(lon) || std::isinf(lat) || std::isinf(lon)) {
        return false;
    }
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

} // namespace

RouteService::RouteService(
    std::shared_ptr<IRouteRepository> repository,
    std::shared_ptr<MapPackageManager> packageManager,
    QObject *parent
) : QObject(parent),
    m_repository(std::move(repository)),
    m_packageManager(std::move(packageManager)),
    m_lastStatusMessage(QStringLiteral("Ready for route calculation.")) {
}

void RouteService::setSnappingToleranceMeters(double meters) {
    if (meters > 0.0 && std::abs(m_snappingToleranceMeters - meters) > 1e-3) {
        m_snappingToleranceMeters = meters;
        emit snappingToleranceChanged();
    }
}

void RouteService::setStatus(const QString &msg) {
    m_lastStatusMessage = msg;
    emit statusMessageChanged();
}

QString RouteService::lastDistanceFormatted() const {
    if (!m_lastResult.success) return QStringLiteral("--");
    if (m_lastResult.totalDistanceMeters < 1000.0) {
        return QStringLiteral("%1 m").arg(static_cast<int>(m_lastResult.totalDistanceMeters));
    }
    return QStringLiteral("%1 km").arg(m_lastResult.totalDistanceMeters / 1000.0, 0, 'f', 2);
}

QString RouteService::lastPathSummary() const {
    if (!m_lastResult.success || m_lastResult.nodeIds.isEmpty()) return QStringLiteral("None");
    if (m_lastResult.nodeIds.size() <= 4) {
        return m_lastResult.nodeIds.join(QStringLiteral(" ➔ "));
    }
    return QStringLiteral("%1 ➔ ... (%2 steps) ➔ %3")
        .arg(m_lastResult.nodeIds.front())
        .arg(m_lastResult.nodeIds.size() - 2)
        .arg(m_lastResult.nodeIds.back());
}

std::optional<QString> RouteService::snapToNearestNode(
    const QString &regionId,
    double lat, double lon,
    double maxToleranceMeters
) const {
    if (!m_repository || !isValidCoordinate(lat, lon)) return std::nullopt;

    auto graphOpt = m_repository->getGraph(regionId);
    if (!graphOpt || graphOpt->nodes.empty()) return std::nullopt;

    double bestDist = std::numeric_limits<double>::infinity();
    std::string bestId;

    for (const auto &node : graphOpt->nodes) {
        const double d = haversineDist(lat, lon, node.latitude, node.longitude);
        if (d < bestDist) {
            bestDist = d;
            bestId = node.id;
        }
    }

    if (bestDist <= maxToleranceMeters && !bestId.empty()) {
        return QString::fromStdString(bestId);
    }
    return std::nullopt;
}

RouteServiceResult RouteService::computeRouteDirect(
    const QString &regionId,
    double startLat, double startLon,
    double destLat, double destLon,
    bool avoidHazards
) {
    RouteServiceResult res;

    // 1. Coordinate Validation
    if (!isValidCoordinate(startLat, startLon)) {
        res.statusMessage = QStringLiteral("Invalid start coordinates: (%1, %2)").arg(startLat).arg(startLon);
        return res;
    }
    if (!isValidCoordinate(destLat, destLon)) {
        res.statusMessage = QStringLiteral("Invalid destination coordinates: (%1, %2)").arg(destLat).arg(destLon);
        return res;
    }

    // 2. Start equals Destination
    if (std::abs(startLat - destLat) < 1e-7 && std::abs(startLon - destLon) < 1e-7) {
        res.success = true;
        res.totalDistanceMeters = 0.0;
        res.totalCostMeters = 0.0;
        res.estimatedDurationSeconds = 0;
        res.statusMessage = QStringLiteral("Origin and destination are identical (0 m).");
        QVariantMap pt;
        pt[QStringLiteral("latitude")] = startLat;
        pt[QStringLiteral("longitude")] = startLon;
        res.pathCoordinates.append(pt);
        return res;
    }

    // 3. Graph Validation
    if (!m_repository) {
        res.statusMessage = QStringLiteral("Routing repository is unavailable.");
        return res;
    }

    auto graphOpt = m_repository->getGraph(regionId);
    if (!graphOpt) {
        res.statusMessage = QStringLiteral("Missing routing graph for region: '%1'.").arg(regionId);
        return res;
    }

    const auto &graph = *graphOpt;
    if (graph.nodes.empty()) {
        res.statusMessage = QStringLiteral("Routing graph for region '%1' is empty (zero nodes).").arg(regionId);
        return res;
    }
    if (graph.version < 0) {
        res.statusMessage = QStringLiteral("Unsupported graph version: %1").arg(graph.version);
        return res;
    }

    // Validate edge weights
    for (const auto &e : graph.edges) {
        if (e.distanceMeters < 0.0 || std::isnan(e.distanceMeters) || std::isinf(e.distanceMeters)) {
            res.statusMessage = QStringLiteral("Graph contains invalid or negative edge distance between %1 and %2")
                .arg(QString::fromStdString(e.fromNodeId), QString::fromStdString(e.toNodeId));
            return res;
        }
    }

    // 4. Endpoint Snapping within tolerance
    auto startNodeOpt = snapToNearestNode(regionId, startLat, startLon, m_snappingToleranceMeters);
    if (!startNodeOpt) {
        res.statusMessage = QStringLiteral("Origin coordinate is outside graph coverage (exceeds snapping tolerance of %1 m).")
            .arg(static_cast<int>(m_snappingToleranceMeters));
        return res;
    }

    auto destNodeOpt = snapToNearestNode(regionId, destLat, destLon, m_snappingToleranceMeters);
    if (!destNodeOpt) {
        res.statusMessage = QStringLiteral("Destination coordinate is outside graph coverage (exceeds snapping tolerance of %1 m).")
            .arg(static_cast<int>(m_snappingToleranceMeters));
        return res;
    }

    const std::string startId = startNodeOpt->toStdString();
    const std::string destId = destNodeOpt->toStdString();

    if (startId == destId) {
        res.success = true;
        res.totalDistanceMeters = 0.0;
        res.statusMessage = QStringLiteral("Origin and destination snapped to the same road node (%1).").arg(*startNodeOpt);
        const auto *n = graph.node(startId);
        if (n) {
            QVariantMap pt;
            pt[QStringLiteral("latitude")] = n->latitude;
            pt[QStringLiteral("longitude")] = n->longitude;
            res.pathCoordinates.append(pt);
        }
        res.nodeIds.append(*startNodeOpt);
        return res;
    }

    // 5. A* Pathfinding execution (Safe Route Pro actively avoids hazards and blocked roads)
    georescux::routing::RouteGraph routingGraph = graph;
    if (!avoidHazards) {
        routingGraph.hazards.clear();
    }
    auto routeOpt = georescux::routing::AStarPathfinder::findRoute(routingGraph, startId, destId);
    if (!routeOpt || routeOpt->empty()) {
        res.statusMessage = QStringLiteral("Destination is unreachable via the available road network.");
        return res;
    }

    const auto &route = *routeOpt;
    res.success = true;
    res.totalDistanceMeters = route.totalDistanceMeters;
    res.totalCostMeters = route.totalCostMeters;
    res.estimatedDurationSeconds = route.estimatedDurationSeconds;

    for (const auto &nid : route.nodeIds) {
        res.nodeIds.append(QString::fromStdString(nid));
        const auto *n = graph.node(nid);
        if (n) {
            QVariantMap pt;
            pt[QStringLiteral("latitude")] = n->latitude;
            pt[QStringLiteral("longitude")] = n->longitude;
            res.pathCoordinates.append(pt);
        }
    }

    // Step-by-step corridor cues
    for (size_t i = 0; i + 1 < route.nodeIds.size(); ++i) {
        const std::string &u = route.nodeIds[i];
        const std::string &v = route.nodeIds[i + 1];
        auto edge = graph.edgeBetween(u, v);
        const double stepDist = edge ? edge->distanceMeters : 0.0;

        QVariantMap step;
        step[QStringLiteral("stepNumber")] = static_cast<int>(i + 1);
        step[QStringLiteral("from")] = QString::fromStdString(u);
        step[QStringLiteral("to")] = QString::fromStdString(v);
        step[QStringLiteral("distanceMeters")] = stepDist;
        step[QStringLiteral("instruction")] = QStringLiteral("Follow road corridor from %1 to %2 (%3 m)")
            .arg(QString::fromStdString(u))
            .arg(QString::fromStdString(v))
            .arg(static_cast<int>(stepDist));
        res.stepInstructions.append(step);
    }

    for (const auto &h : route.hazardWarnings) {
        res.hazardWarnings.append(QString::fromStdString(h.id));
    }

    res.statusMessage = QStringLiteral("Route calculated: %1 km (%2 steps)")
        .arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2)
        .arg(route.nodeIds.size());
    return res;
}

bool RouteService::calculateRoute(
    const QString &regionId,
    double startLat, double startLon,
    double destLat, double destLon,
    bool avoidHazards
) {
    m_isCalculating = true;
    emit calculationStateChanged();

    m_lastResult = computeRouteDirect(regionId, startLat, startLon, destLat, destLon, avoidHazards);

    m_isCalculating = false;
    emit calculationStateChanged();
    emit routeChanged();
    setStatus(m_lastResult.statusMessage);
    emit routeComputed(m_lastResult.success, m_lastResult.totalDistanceMeters, m_lastResult.statusMessage);

    return m_lastResult.success;
}

bool RouteService::calculateRouteByNodes(
    const QString &regionId,
    const QString &startNodeId,
    const QString &destNodeId,
    bool avoidHazards
) {
    m_isCalculating = true;
    emit calculationStateChanged();

    RouteServiceResult res;
    if (!m_repository) {
        res.statusMessage = QStringLiteral("Routing repository unavailable.");
        m_lastResult = res;
        m_isCalculating = false;
        emit calculationStateChanged();
        emit routeChanged();
        setStatus(res.statusMessage);
        return false;
    }

    auto graphOpt = m_repository->getGraph(regionId);
    if (!graphOpt) {
        res.statusMessage = QStringLiteral("Missing routing graph for region: %1").arg(regionId);
        m_lastResult = res;
        m_isCalculating = false;
        emit calculationStateChanged();
        emit routeChanged();
        setStatus(res.statusMessage);
        return false;
    }

    const auto &graph = *graphOpt;
    const std::string sId = startNodeId.toStdString();
    const std::string dId = destNodeId.toStdString();

    if (sId == dId) {
        res.success = true;
        res.totalDistanceMeters = 0.0;
        res.nodeIds.append(startNodeId);
        res.statusMessage = QStringLiteral("Start and destination are identical.");
        m_lastResult = res;
        m_isCalculating = false;
        emit calculationStateChanged();
        emit routeChanged();
        setStatus(res.statusMessage);
        return true;
    }

    georescux::routing::RouteGraph routingGraph = graph;
    if (!avoidHazards) {
        routingGraph.hazards.clear();
    }
    auto routeOpt = georescux::routing::AStarPathfinder::findRoute(routingGraph, sId, dId);
    if (!routeOpt || routeOpt->empty()) {
        res.statusMessage = QStringLiteral("Destination is unreachable via the available road network.");
        m_lastResult = res;
        m_isCalculating = false;
        emit calculationStateChanged();
        emit routeChanged();
        setStatus(res.statusMessage);
        return false;
    }

    const auto &route = *routeOpt;
    res.success = true;
    res.totalDistanceMeters = route.totalDistanceMeters;
    res.totalCostMeters = route.totalCostMeters;
    res.estimatedDurationSeconds = route.estimatedDurationSeconds;

    for (const auto &nid : route.nodeIds) {
        res.nodeIds.append(QString::fromStdString(nid));
        const auto *n = graph.node(nid);
        if (n) {
            QVariantMap pt;
            pt[QStringLiteral("latitude")] = n->latitude;
            pt[QStringLiteral("longitude")] = n->longitude;
            res.pathCoordinates.append(pt);
        }
    }

    res.statusMessage = QStringLiteral("Route calculated: %1 km").arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2);
    m_lastResult = res;
    m_isCalculating = false;
    emit calculationStateChanged();
    emit routeChanged();
    setStatus(res.statusMessage);
    emit routeComputed(true, res.totalDistanceMeters, res.statusMessage);
    return true;
}

void RouteService::clearRoute() {
    m_lastResult = RouteServiceResult();
    setStatus(QStringLiteral("Route cleared."));
    emit routeChanged();
}

} // namespace georescux::domain
