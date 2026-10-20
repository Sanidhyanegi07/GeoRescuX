#include "OfflineMapView.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>

namespace georescux::ui {

OfflineMapView::OfflineMapView(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

double OfflineMapView::pixelsPerDegree() const {
    // 1 degree latitude ~= 111,000 meters.
    // Base scale at zoom 1.0 is ~50 pixels per degree.
    return 50.0 * std::pow(1.5, m_zoom);
}

QPointF OfflineMapView::geoToPixel(double lat, double lon) const {
    const double ppd = pixelsPerDegree();
    const double cosLat = std::cos(m_centerLat * M_PI / 180.0);
    const double x = (width() / 2.0) + (lon - m_centerLon) * ppd * cosLat;
    const double y = (height() / 2.0) - (lat - m_centerLat) * ppd;
    return QPointF(x, y);
}

QPointF OfflineMapView::pixelToGeo(double px, double py) const {
    const double ppd = pixelsPerDegree();
    if (ppd <= 0.0) return QPointF(m_centerLat, m_centerLon);
    const double cosLat = std::cos(m_centerLat * M_PI / 180.0);
    const double safeCos = std::abs(cosLat) > 1e-6 ? cosLat : 1.0;
    const double lon = m_centerLon + (px - (width() / 2.0)) / (ppd * safeCos);
    const double lat = m_centerLat - (py - (height() / 2.0)) / ppd;
    return QPointF(lat, lon);
}

void OfflineMapView::setRegionId(const QString &id) {
    if (m_regionId != id) {
        m_regionId = id;
        emit regionIdChanged();
        update();
    }
}

void OfflineMapView::setCenterLatitude(double lat) {
    if (std::abs(m_centerLat - lat) > 1e-7) {
        m_centerLat = lat;
        emit centerChanged();
        update();
    }
}

void OfflineMapView::setCenterLongitude(double lon) {
    if (std::abs(m_centerLon - lon) > 1e-7) {
        m_centerLon = lon;
        emit centerChanged();
        update();
    }
}

void OfflineMapView::setZoomLevel(double z) {
    const double clamped = std::clamp(z, 2.0, 18.0);
    if (std::abs(m_zoom - clamped) > 1e-4) {
        m_zoom = clamped;
        emit zoomChanged();
        update();
    }
}

void OfflineMapView::zoomIn() {
    setZoomLevel(m_zoom + 0.8);
}

void OfflineMapView::zoomOut() {
    setZoomLevel(m_zoom - 0.8);
}

void OfflineMapView::resetView() {
    if (m_coverage.isValid()) {
        m_centerLat = (m_coverage.minLatitude + m_coverage.maxLatitude) / 2.0;
        m_centerLon = (m_coverage.minLongitude + m_coverage.maxLongitude) / 2.0;
        m_zoom = 6.5;
    } else {
        m_centerLat = 30.3165;
        m_centerLon = 78.0322;
        m_zoom = 8.0;
    }
    emit centerChanged();
    emit zoomChanged();
    update();
}

void OfflineMapView::panBy(double dxPixels, double dyPixels) {
    const double ppd = pixelsPerDegree();
    if (ppd <= 0.0) return;
    const double cosLat = std::cos(m_centerLat * M_PI / 180.0);
    const double safeCos = std::abs(cosLat) > 1e-6 ? cosLat : 1.0;
    m_centerLon -= dxPixels / (ppd * safeCos);
    m_centerLat += dyPixels / ppd;
    emit centerChanged();
    update();
}

void OfflineMapView::centerOn(double lat, double lon) {
    m_centerLat = lat;
    m_centerLon = lon;
    emit centerChanged();
    update();
}

void OfflineMapView::fitBounds(double minLat, double maxLat, double minLon, double maxLon) {
    if (minLat >= maxLat || minLon >= maxLon) return;
    m_centerLat = (minLat + maxLat) / 2.0;
    m_centerLon = (minLon + maxLon) / 2.0;

    const double latSpan = maxLat - minLat;
    const double lonSpan = maxLon - minLon;
    // Estimate zoom to fit screen
    if (width() > 0 && height() > 0 && latSpan > 0 && lonSpan > 0) {
        const double ppdY = height() / latSpan;
        const double ppdX = width() / (lonSpan * std::cos(m_centerLat * M_PI / 180.0));
        const double targetPpd = std::min(ppdX, ppdY) * 0.85;
        m_zoom = std::clamp(std::log2(targetPpd / 50.0) / std::log2(1.5), 2.0, 16.0);
    }
    emit centerChanged();
    emit zoomChanged();
    update();
}

QString OfflineMapView::routeDistanceFormatted() const {
    if (m_routeDistanceMeters <= 0.0) return QStringLiteral("--");
    if (m_routeDistanceMeters < 1000.0) {
        return QStringLiteral("%1 m").arg(static_cast<int>(m_routeDistanceMeters));
    }
    return QStringLiteral("%1 km").arg(m_routeDistanceMeters / 1000.0, 0, 'f', 1);
}

void OfflineMapView::setRouteCoordinates(const QVariantList &latLonPairs, double distanceMeters) {
    m_routePolyline.clear();
    m_routeDistanceMeters = distanceMeters;

    for (const auto &item : latLonPairs) {
        if (item.canConvert<QVariantMap>()) {
            const QVariantMap m = item.toMap();
            const double lat = m.value(QStringLiteral("latitude"), m.value(QStringLiteral("lat"))).toDouble();
            const double lon = m.value(QStringLiteral("longitude"), m.value(QStringLiteral("lon"))).toDouble();
            if (lat != 0.0 && lon != 0.0) {
                m_routePolyline.emplace_back(lat, lon);
            }
        }
    }

    emit routeOverlayChanged();
    update();
}

void OfflineMapView::clearRouteOverlay() {
    m_routePolyline.clear();
    m_routeDistanceMeters = 0.0;
    emit routeOverlayChanged();
    update();
}

bool OfflineMapView::loadRegionData(
    const QString &regionId,
    const QString &displayName,
    const QString &boundaryPath,
    const QString &graphPath,
    const QString &placesPath,
    double minLat, double maxLat,
    double minLon, double maxLon
) {
    m_regionId = regionId;
    m_regionDisplayName = displayName.isEmpty() ? regionId : displayName;
    m_roads.clear();
    m_places.clear();
    m_boundaryPolygon.clear();

    m_coverage.minLatitude = minLat;
    m_coverage.maxLatitude = maxLat;
    m_coverage.minLongitude = minLon;
    m_coverage.maxLongitude = maxLon;

    // 1. Boundary GeoJSON parsing
    if (!boundaryPath.isEmpty() && QFile::exists(boundaryPath)) {
        QFile bf(boundaryPath);
        if (bf.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(bf.readAll());
            const QJsonObject root = doc.object();
            const QJsonArray features = root.value(QStringLiteral("features")).toArray();
            for (const auto &feat : features) {
                const QJsonObject geom = feat.toObject().value(QStringLiteral("geometry")).toObject();
                const QString geomType = geom.value(QStringLiteral("type")).toString();
                if (geomType == QStringLiteral("Polygon")) {
                    const QJsonArray rings = geom.value(QStringLiteral("coordinates")).toArray();
                    if (!rings.isEmpty()) {
                        const QJsonArray outer = rings.at(0).toArray();
                        for (const auto &coord : outer) {
                            const QJsonArray pt = coord.toArray();
                            if (pt.size() >= 2) {
                                m_boundaryPolygon.append(QPointF(pt.at(0).toDouble(), pt.at(1).toDouble())); // (lon, lat)
                            }
                        }
                    }
                }
            }
            bf.close();
        }
    }

    // 2. Road Network Graph parsing
    if (!graphPath.isEmpty() && QFile::exists(graphPath)) {
        QFile gf(graphPath);
        if (gf.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(gf.readAll());
            const QJsonObject root = doc.object();
            const QJsonArray nodes = root.value(QStringLiteral("nodes")).toArray();
            const QJsonArray edges = root.value(QStringLiteral("edges")).toArray();

            std::unordered_map<QString, QPointF> nodeCoords;
            for (const auto &n : nodes) {
                const QJsonObject no = n.toObject();
                const QString nid = no.value(QStringLiteral("id")).toString();
                const double nlat = no.value(QStringLiteral("latitude")).toDouble();
                const double nlon = no.value(QStringLiteral("longitude")).toDouble();
                nodeCoords[nid] = QPointF(nlat, nlon);
            }

            // Cap road segments to keep rendering extremely smooth while sampling uniformly
            // across the entire regional dataset to prevent district bias or starving eastern areas.
            const int totalEdges = edges.size();
            const int maxEdges = std::min(totalEdges, 12000);
            const double stride = (totalEdges > maxEdges && maxEdges > 0)
                ? static_cast<double>(totalEdges) / maxEdges
                : 1.0;

            for (int i = 0; i < maxEdges; ++i) {
                const int edgeIdx = std::min(static_cast<int>(i * stride), totalEdges - 1);
                const QJsonObject eo = edges.at(edgeIdx).toObject();
                const QString fromId = eo.value(QStringLiteral("fromNodeId")).toString();
                const QString toId = eo.value(QStringLiteral("toNodeId")).toString();

                auto itFrom = nodeCoords.find(fromId);
                auto itTo = nodeCoords.find(toId);
                if (itFrom != nodeCoords.end() && itTo != nodeCoords.end()) {
                    MapRoadSegment seg;
                    seg.lat1 = itFrom->second.x();
                    seg.lon1 = itFrom->second.y();
                    seg.lat2 = itTo->second.x();
                    seg.lon2 = itTo->second.y();
                    seg.isEvacuationCorridor = eo.value(QStringLiteral("isCorridor")).toBool(false);
                    m_roads.push_back(seg);
                }
            }
            gf.close();
        }
    }

    // 3. Places / POIs parsing: prioritize safe havens and hospitals for emergency response
    if (!placesPath.isEmpty() && QFile::exists(placesPath)) {
        QFile pf(placesPath);
        if (pf.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(pf.readAll());
            const QJsonArray places = doc.isArray() ? doc.array() : doc.object().value(QStringLiteral("places")).toArray();

            constexpr int maxPlaces = 500;
            std::vector<MapPlaceMarker> safeHavens;
            std::vector<MapPlaceMarker> hospitals;
            std::vector<MapPlaceMarker> generalPlaces;

            for (const auto &p : places) {
                const QJsonObject po = p.toObject();
                MapPlaceMarker marker;
                marker.name = po.value(QStringLiteral("name")).toString();
                marker.kind = po.value(QStringLiteral("kind")).toString();
                marker.latitude = po.value(QStringLiteral("latitude")).toDouble();
                marker.longitude = po.value(QStringLiteral("longitude")).toDouble();
                marker.isSafeHaven = po.value(QStringLiteral("isSafeHaven")).toBool(false);
                if (marker.latitude == 0.0 || marker.longitude == 0.0 || marker.name.isEmpty()) {
                    continue;
                }
                if (marker.isSafeHaven) {
                    safeHavens.push_back(marker);
                } else if (marker.kind == QStringLiteral("hospital")) {
                    hospitals.push_back(marker);
                } else {
                    generalPlaces.push_back(marker);
                }
            }

            // Assemble prioritized places up to maxPlaces
            for (const auto &sh : safeHavens) {
                if (static_cast<int>(m_places.size()) >= maxPlaces) break;
                m_places.push_back(sh);
            }
            for (const auto &h : hospitals) {
                if (static_cast<int>(m_places.size()) >= maxPlaces) break;
                m_places.push_back(h);
            }
            for (const auto &g : generalPlaces) {
                if (static_cast<int>(m_places.size()) >= maxPlaces) break;
                m_places.push_back(g);
            }
            pf.close();
        }
    }

    // Auto-center on region center
    if (m_coverage.isValid()) {
        m_centerLat = (m_coverage.minLatitude + m_coverage.maxLatitude) / 2.0;
        m_centerLon = (m_coverage.minLongitude + m_coverage.maxLongitude) / 2.0;
        m_zoom = 6.2;
    } else if (!m_roads.empty()) {
        m_centerLat = m_roads.front().lat1;
        m_centerLon = m_roads.front().lon1;
        m_zoom = 8.0;
    }

    if (m_roads.empty() && m_boundaryPolygon.isEmpty() && m_places.empty()) {
        m_viewState = MapViewState::InvalidData;
        m_statusMessage = QStringLiteral("No geographic features could be loaded for region: ") + m_regionDisplayName;
    } else {
        m_viewState = MapViewState::Ready;
        m_statusMessage = QStringLiteral("Offline Map Active: %1 (%2 roads, %3 POIs)")
            .arg(m_regionDisplayName)
            .arg(m_roads.size())
            .arg(m_places.size());
    }

    emit regionIdChanged();
    emit regionDisplayNameChanged();
    emit centerChanged();
    emit zoomChanged();
    emit viewStateChanged();
    emit statusMessageChanged();
    emit contentMetricsChanged();
    update();
    return true;
}

void OfflineMapView::updateStatus(MapViewState state, const QString &msg) {
    m_viewState = state;
    m_statusMessage = msg;
    emit viewStateChanged();
    emit statusMessageChanged();
    update();
}

void OfflineMapView::paint(QPainter *painter) {
    if (!painter) return;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF rect(0, 0, width(), height());

    // 1. Base dark emergency canvas background
    painter->fillRect(rect, QColor(0x0A, 0x0F, 0x1D));

    // Handle NoPackage or Corrupt states
    if (m_viewState == MapViewState::NoPackageInstalled) {
        painter->setPen(QColor(0x64, 0x74, 0x8B));
        QFont f = painter->font();
        f.setPixelSize(14);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(rect, Qt::AlignCenter, QStringLiteral("No Offline Map Package Installed\nSelect a regional package to begin."));
        return;
    }

    if (m_viewState == MapViewState::InvalidData) {
        painter->setPen(QColor(0xEF, 0x44, 0x44));
        QFont f = painter->font();
        f.setPixelSize(14);
        f.setBold(true);
        painter->setFont(f);
        painter->drawText(rect, Qt::AlignCenter, QStringLiteral("Invalid or Corrupted Map Data\n%1").arg(m_statusMessage));
        return;
    }

    // 2. Draw Regional Boundary Polygon if available
    if (!m_boundaryPolygon.isEmpty()) {
        QPolygonF canvasPoly;
        for (const auto &pt : m_boundaryPolygon) {
            // pt.x() is lon, pt.y() is lat
            canvasPoly.append(geoToPixel(pt.y(), pt.x()));
        }
        // Fill region terrain
        painter->setBrush(QColor(0x0F, 0x1A, 0x2E));
        painter->setPen(QPen(QColor(0x02, 0x84, 0xC7), 2.0, Qt::DashLine));
        painter->drawPolygon(canvasPoly);
    }

    // 3. Draw Geographic Lat/Lon Grid Lines
    painter->setPen(QPen(QColor(0x1E, 0x29, 0x3B, 120), 1.0, Qt::DotLine));
    QFont gridFont = painter->font();
    gridFont.setPixelSize(9);
    painter->setFont(gridFont);

    // Calculate grid step based on zoom
    const double gridStep = (m_zoom > 10.0) ? 0.1 : (m_zoom > 6.0) ? 0.5 : 1.0;
    const double minLatGrid = std::floor((m_centerLat - 3.0) / gridStep) * gridStep;
    const double maxLatGrid = std::ceil((m_centerLat + 3.0) / gridStep) * gridStep;
    const double minLonGrid = std::floor((m_centerLon - 3.0) / gridStep) * gridStep;
    const double maxLonGrid = std::ceil((m_centerLon + 3.0) / gridStep) * gridStep;

    for (double lat = minLatGrid; lat <= maxLatGrid; lat += gridStep) {
        const QPointF p1 = geoToPixel(lat, m_centerLon - 5.0);
        const QPointF p2 = geoToPixel(lat, m_centerLon + 5.0);
        painter->drawLine(p1, p2);
        painter->setPen(QColor(0x47, 0x55, 0x69));
        painter->drawText(10, static_cast<int>(p1.y()) - 4, QString::number(lat, 'f', 1) + QStringLiteral("°N"));
        painter->setPen(QPen(QColor(0x1E, 0x29, 0x3B, 120), 1.0, Qt::DotLine));
    }
    for (double lon = minLonGrid; lon <= maxLonGrid; lon += gridStep) {
        const QPointF p1 = geoToPixel(m_centerLat - 5.0, lon);
        const QPointF p2 = geoToPixel(m_centerLat + 5.0, lon);
        painter->drawLine(p1, p2);
        painter->setPen(QColor(0x47, 0x55, 0x69));
        painter->drawText(static_cast<int>(p1.x()) + 4, height() - 10, QString::number(lon, 'f', 1) + QStringLiteral("°E"));
        painter->setPen(QPen(QColor(0x1E, 0x29, 0x3B, 120), 1.0, Qt::DotLine));
    }

    // 4. Draw Road Network Segments
    if (!m_roads.empty()) {
        const QPen roadPen(QColor(0x33, 0x41, 0x55), 1.5, Qt::SolidLine, Qt::RoundCap);
        const QPen corridorPen(QColor(0x02, 0x84, 0xC7), 2.2, Qt::SolidLine, Qt::RoundCap);

        for (const auto &road : m_roads) {
            const QPointF p1 = geoToPixel(road.lat1, road.lon1);
            const QPointF p2 = geoToPixel(road.lat2, road.lon2);

            // Simple viewport clipping check
            if ((p1.x() < -50 && p2.x() < -50) || (p1.x() > width() + 50 && p2.x() > width() + 50) ||
                (p1.y() < -50 && p2.y() < -50) || (p1.y() > height() + 50 && p2.y() > height() + 50)) {
                continue;
            }

            painter->setPen(road.isEvacuationCorridor ? corridorPen : roadPen);
            painter->drawLine(p1, p2);
        }
    }

    // 5. Draw Calculated Route Polyline Overlay (from Phase 7 routing)
    if (!m_routePolyline.empty()) {
        // Outer glow
        painter->setPen(QPen(QColor(0x06, 0xB6, 0xD4, 100), 7.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (size_t i = 0; i + 1 < m_routePolyline.size(); ++i) {
            const QPointF p1 = geoToPixel(m_routePolyline[i].x(), m_routePolyline[i].y());
            const QPointF p2 = geoToPixel(m_routePolyline[i + 1].x(), m_routePolyline[i + 1].y());
            painter->drawLine(p1, p2);
        }
        // Inner sharp route line
        painter->setPen(QPen(QColor(0x38, 0xBD, 0xF8), 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        for (size_t i = 0; i + 1 < m_routePolyline.size(); ++i) {
            const QPointF p1 = geoToPixel(m_routePolyline[i].x(), m_routePolyline[i].y());
            const QPointF p2 = geoToPixel(m_routePolyline[i + 1].x(), m_routePolyline[i + 1].y());
            painter->drawLine(p1, p2);
        }

        // Start Node Marker (Green)
        const QPointF startP = geoToPixel(m_routePolyline.front().x(), m_routePolyline.front().y());
        painter->setBrush(QColor(0x10, 0xB9, 0x81));
        painter->setPen(QPen(Qt::white, 1.5));
        painter->drawEllipse(startP, 7.0, 7.0);

        // Destination Node Marker (Red)
        const QPointF endP = geoToPixel(m_routePolyline.back().x(), m_routePolyline.back().y());
        painter->setBrush(QColor(0xEF, 0x44, 0x44));
        painter->setPen(QPen(Qt::white, 1.5));
        painter->drawEllipse(endP, 7.0, 7.0);
    }

    // 6. Draw Places & POI Markers
    if (!m_places.empty()) {
        QFont placeFont = painter->font();
        placeFont.setPixelSize(10);
        placeFont.setBold(false);
        painter->setFont(placeFont);

        for (const auto &place : m_places) {
            const QPointF pt = geoToPixel(place.latitude, place.longitude);
            if (pt.x() < -20 || pt.x() > width() + 20 || pt.y() < -20 || pt.y() > height() + 20) {
                continue;
            }

            if (place.isSafeHaven || place.kind == QStringLiteral("hospital")) {
                // High-visibility Emergency Shelter / Hospital marker
                const QColor markerColor = place.isSafeHaven ? QColor(0x10, 0xB9, 0x81) : QColor(0xEF, 0x44, 0x44);
                painter->setBrush(markerColor);
                painter->setPen(QPen(Qt::white, 1.0));
                painter->drawRoundedRect(QRectF(pt.x() - 5, pt.y() - 5, 10, 10), 2, 2);

                if (m_zoom >= 7.0) {
                    painter->setPen(markerColor);
                    painter->drawText(static_cast<int>(pt.x()) + 8, static_cast<int>(pt.y()) + 4, place.name);
                }
            } else if (m_zoom >= 8.5) {
                // City / Village dot
                painter->setBrush(QColor(0x94, 0xA3, 0xB8));
                painter->setPen(Qt::NoPen);
                painter->drawEllipse(pt, 3.0, 3.0);
                painter->setPen(QColor(0xCB, 0xD5, 0xE1));
                painter->drawText(static_cast<int>(pt.x()) + 6, static_cast<int>(pt.y()) + 3, place.name);
            }
        }
    }

    // 7. Map Attribution & Offline Status Badge (Top-Right)
    const QString badgeText = QStringLiteral("OFFLINE VECTOR MAP (ZERO NETWORK)");
    QFont badgeFont = painter->font();
    badgeFont.setPixelSize(10);
    badgeFont.setBold(true);
    painter->setFont(badgeFont);
    const QFontMetrics fm(badgeFont);
    const int badgeW = fm.horizontalAdvance(badgeText) + 16;
    const QRectF badgeRect(width() - badgeW - 12, 12, badgeW, 24);

    painter->setBrush(QColor(0x06, 0x4E, 0x3B, 220));
    painter->setPen(QPen(QColor(0x10, 0xB9, 0x81), 1.0));
    painter->drawRoundedRect(badgeRect, 6, 6);
    painter->setPen(QColor(0x6E, 0xEE, 0xB8));
    painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

    // 8. Scale / Coordinate Indicator (Bottom-Left)
    const QString coordStr = QStringLiteral("%1°N, %2°E | Zoom: %3x")
        .arg(m_centerLat, 0, 'f', 4)
        .arg(m_centerLon, 0, 'f', 4)
        .arg(m_zoom, 0, 'f', 1);
    painter->setPen(QColor(0x64, 0x74, 0x8B));
    painter->drawText(14, height() - 14, coordStr);

    // 9. Draw Live GPS User Position Marker (Phase 8)
    if (m_hasUserPos) {
        const QPointF userPt = geoToPixel(m_userLat, m_userLon);

        // Accuracy halo: calculate radius in screen pixels
        const double ppd = pixelsPerDegree();
        const double metersPerDegree = 111319.5;
        const double radiusPixels = std::clamp((m_userAcc / metersPerDegree) * ppd, 8.0, 180.0);

        // Translucent accuracy circle
        if (m_userStale) {
            painter->setBrush(QColor(0xD9, 0x77, 0x06, 35)); // Translucent amber
            painter->setPen(QPen(QColor(0xF5, 0x9E, 0x0B, 180), 1.2, Qt::DashLine));
        } else {
            painter->setBrush(QColor(0x02, 0x84, 0xC7, 45)); // Translucent cyan/blue
            painter->setPen(QPen(QColor(0x38, 0xBD, 0xF8, 180), 1.2, Qt::SolidLine));
        }
        painter->drawEllipse(userPt, radiusPixels, radiusPixels);

        // Outer pulse ring
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(m_userStale ? QColor(0xF5, 0x9E, 0x0B, 140) : QColor(0x38, 0xBD, 0xF8, 160), 2.0));
        painter->drawEllipse(userPt, 11.0, 11.0);

        // Solid location core dot
        painter->setBrush(m_userStale ? QColor(0xD9, 0x77, 0x06) : QColor(0x02, 0x84, 0xC7));
        painter->setPen(QPen(Qt::white, 2.0));
        painter->drawEllipse(userPt, 6.0, 6.0);

        // Center highlight
        painter->setBrush(Qt::white);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(userPt, 2.0, 2.0);

        // Marker label
        QFont userFont = painter->font();
        userFont.setPixelSize(10);
        userFont.setBold(true);
        painter->setFont(userFont);
        painter->setPen(m_userStale ? QColor(0xFD, 0xE6, 0x8A) : QColor(0xBA, 0xE6, 0xFD));
        const QString label = m_userStale
            ? QStringLiteral("You (Last Known ±%1m)").arg(m_userAcc, 0, 'f', 0)
            : QStringLiteral("You (±%1m)").arg(m_userAcc, 0, 'f', 1);
        painter->drawText(static_cast<int>(userPt.x()) + 14, static_cast<int>(userPt.y()) + 4, label);
    }
}

void OfflineMapView::setUserPosition(double lat, double lon, double accuracyMeters, bool isLive, bool isStale) {
    const bool posChanged = !m_hasUserPos ||
        std::abs(m_userLat - lat) > 0.00001 ||
        std::abs(m_userLon - lon) > 0.00001 ||
        std::abs(m_userAcc - accuracyMeters) > 1.0 ||
        m_userLive != isLive ||
        m_userStale != isStale;

    m_hasUserPos = true;
    m_userLat = lat;
    m_userLon = lon;
    m_userAcc = accuracyMeters;
    m_userLive = isLive;
    m_userStale = isStale;

    if (m_viewState == MapViewState::Ready && m_coverage.isValid()) {
        const bool outside = (lat < m_coverage.minLatitude || lat > m_coverage.maxLatitude ||
                              lon < m_coverage.minLongitude || lon > m_coverage.maxLongitude);
        if (m_isUserOutsideCoverage != outside) {
            m_isUserOutsideCoverage = outside;
            emit userCoverageChanged(m_isUserOutsideCoverage);
        }
    }

    if (posChanged) {
        emit userPositionChanged();
        update();
    }
}

void OfflineMapView::clearUserPosition() {
    if (m_hasUserPos) {
        m_hasUserPos = false;
        m_isUserOutsideCoverage = false;
        emit userPositionChanged();
        emit userCoverageChanged(false);
        update();
    }
}

void OfflineMapView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->position();
        event->accept();
    }
}

void OfflineMapView::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDragging) {
        const QPointF delta = event->position() - m_lastMousePos;
        m_lastMousePos = event->position();
        panBy(delta.x(), delta.y());
        event->accept();
    }
}

void OfflineMapView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        const QPointF geo = pixelToGeo(event->position().x(), event->position().y());
        emit mapClicked(geo.x(), geo.y()); // (lat, lon)
        event->accept();
    }
}

void OfflineMapView::wheelEvent(QWheelEvent *event) {
    const double numDegrees = event->angleDelta().y() / 8.0;
    const double numSteps = numDegrees / 15.0;
    setZoomLevel(m_zoom + numSteps * 0.4);
    event->accept();
}

} // namespace georescux::ui
