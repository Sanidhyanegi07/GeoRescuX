#ifndef GEORESCUX_OFFLINE_MAP_VIEW_H
#define GEORESCUX_OFFLINE_MAP_VIEW_H

#include <QQuickPaintedItem>
#include <QPainter>
#include <QVector2D>
#include <QPolygonF>
#include <vector>
#include <memory>
#include "domain/MapPackageModel.h"

namespace georescux::ui {

enum class MapViewState {
    NoPackageInstalled = 0,
    Loading = 1,
    Ready = 2,
    InvalidData = 3,
    CoverageWarning = 4
};

struct MapRoadSegment {
    double lat1{0.0}, lon1{0.0};
    double lat2{0.0}, lon2{0.0};
    bool isEvacuationCorridor{false};
};

struct MapPlaceMarker {
    QString name;
    QString kind; // "city", "village", "hospital", "safe_haven"
    double latitude{0.0};
    double longitude{0.0};
    bool isSafeHaven{false};
};

/**
 * @brief Zero-network, offline-first vector & geographic feature map item for Qt Quick.
 *
 * Renders real regional road networks, boundary polygons, hospitals,
 * safe havens, and calculated evacuation routes directly via QPainter.
 */
class OfflineMapView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString regionId READ regionId WRITE setRegionId NOTIFY regionIdChanged)
    Q_PROPERTY(QString regionDisplayName READ regionDisplayName NOTIFY regionDisplayNameChanged)
    Q_PROPERTY(double centerLatitude READ centerLatitude WRITE setCenterLatitude NOTIFY centerChanged)
    Q_PROPERTY(double centerLongitude READ centerLongitude WRITE setCenterLongitude NOTIFY centerChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomChanged)
    Q_PROPERTY(int viewState READ viewState NOTIFY viewStateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int renderedRoadCount READ renderedRoadCount NOTIFY contentMetricsChanged)
    Q_PROPERTY(int renderedPlaceCount READ renderedPlaceCount NOTIFY contentMetricsChanged)
    Q_PROPERTY(bool hasRouteOverlay READ hasRouteOverlay NOTIFY routeOverlayChanged)
    Q_PROPERTY(double routeDistanceMeters READ routeDistanceMeters NOTIFY routeOverlayChanged)
    Q_PROPERTY(QString routeDistanceFormatted READ routeDistanceFormatted NOTIFY routeOverlayChanged)

public:
    explicit OfflineMapView(QQuickItem *parent = nullptr);
    ~OfflineMapView() override = default;

    void paint(QPainter *painter) override;

    [[nodiscard]] QString regionId() const { return m_regionId; }
    void setRegionId(const QString &id);

    [[nodiscard]] QString regionDisplayName() const { return m_regionDisplayName; }
    [[nodiscard]] double centerLatitude() const { return m_centerLat; }
    void setCenterLatitude(double lat);
    [[nodiscard]] double centerLongitude() const { return m_centerLon; }
    void setCenterLongitude(double lon);
    [[nodiscard]] double zoomLevel() const { return m_zoom; }
    void setZoomLevel(double z);

    [[nodiscard]] int viewState() const { return static_cast<int>(m_viewState); }
    [[nodiscard]] QString statusMessage() const { return m_statusMessage; }
    [[nodiscard]] int renderedRoadCount() const { return static_cast<int>(m_roads.size()); }
    [[nodiscard]] int renderedPlaceCount() const { return static_cast<int>(m_places.size()); }
    [[nodiscard]] bool hasRouteOverlay() const { return !m_routePolyline.empty(); }
    [[nodiscard]] double routeDistanceMeters() const { return m_routeDistanceMeters; }
    [[nodiscard]] QString routeDistanceFormatted() const;

    // Interactive operations
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void panBy(double dxPixels, double dyPixels);
    Q_INVOKABLE void centerOn(double lat, double lon);
    Q_INVOKABLE void fitBounds(double minLat, double maxLat, double minLon, double maxLon);

    // Data ingestion
    Q_INVOKABLE bool loadRegionData(
        const QString &regionId,
        const QString &displayName,
        const QString &boundaryPath,
        const QString &graphPath,
        const QString &placesPath,
        double minLat = 0.0, double maxLat = 0.0,
        double minLon = 0.0, double maxLon = 0.0
    );

    // Route overlay
    Q_INVOKABLE void setRouteCoordinates(const QVariantList &latLonPairs, double distanceMeters);
    Q_INVOKABLE void clearRouteOverlay();

    // Coordinate conversion
    [[nodiscard]] QPointF geoToPixel(double lat, double lon) const;
    [[nodiscard]] QPointF pixelToGeo(double px, double py) const;

signals:
    void regionIdChanged();
    void regionDisplayNameChanged();
    void centerChanged();
    void zoomChanged();
    void viewStateChanged();
    void statusMessageChanged();
    void contentMetricsChanged();
    void routeOverlayChanged();
    void mapClicked(double lat, double lon);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QString m_regionId;
    QString m_regionDisplayName;
    double m_centerLat{30.3165};
    double m_centerLon{78.0322};
    double m_zoom{10.0}; // Zoom scale factor

    georescux::domain::GeographicCoverage m_coverage;
    MapViewState m_viewState{MapViewState::NoPackageInstalled};
    QString m_statusMessage{QStringLiteral("No offline map package active")};

    // Geographic datasets
    QPolygonF m_boundaryPolygon;
    std::vector<MapRoadSegment> m_roads;
    std::vector<MapPlaceMarker> m_places;

    // Route Overlay
    std::vector<QPointF> m_routePolyline; // (lat, lon)
    double m_routeDistanceMeters{0.0};

    // Mouse Interaction
    bool m_isDragging{false};
    QPointF m_lastMousePos;

    void updateStatus(MapViewState state, const QString &msg);
    double pixelsPerDegree() const;
};

} // namespace georescux::ui

#endif // GEORESCUX_OFFLINE_MAP_VIEW_H
