#ifndef GEORESCUX_ROUTE_SERVICE_H
#define GEORESCUX_ROUTE_SERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <memory>
#include <optional>
#include "IRouteRepository.h"
#include "MapPackageManager.h"
#include "AStarPathfinder.h"

namespace georescux::domain {

struct RouteServiceResult {
    bool success{false};
    QString statusMessage;
    double totalDistanceMeters{0.0};
    double totalCostMeters{0.0};
    qint64 estimatedDurationSeconds{0};
    QStringList nodeIds;
    QVariantList pathCoordinates; // List of QVariantMap { "latitude", "longitude" }
    QVariantList stepInstructions; // List of QVariantMap { "stepNumber", "from", "to", "instruction", "distanceMeters" }
    QStringList hazardWarnings;
};

class RouteService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isCalculating READ isCalculating NOTIFY calculationStateChanged)
    Q_PROPERTY(bool hasRoute READ hasRoute NOTIFY routeChanged)
    Q_PROPERTY(double lastDistanceMeters READ lastDistanceMeters NOTIFY routeChanged)
    Q_PROPERTY(QString lastDistanceFormatted READ lastDistanceFormatted NOTIFY routeChanged)
    Q_PROPERTY(int lastDurationMinutes READ lastDurationMinutes NOTIFY routeChanged)
    Q_PROPERTY(QString lastStatusMessage READ lastStatusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastPathSummary READ lastPathSummary NOTIFY routeChanged)
    Q_PROPERTY(QVariantList lastCoordinates READ lastCoordinates NOTIFY routeChanged)
    Q_PROPERTY(QVariantList lastSteps READ lastSteps NOTIFY routeChanged)
    Q_PROPERTY(double snappingToleranceMeters READ snappingToleranceMeters WRITE setSnappingToleranceMeters NOTIFY snappingToleranceChanged)

public:
    explicit RouteService(
        std::shared_ptr<IRouteRepository> repository,
        std::shared_ptr<MapPackageManager> packageManager = nullptr,
        QObject *parent = nullptr
    );
    ~RouteService() override = default;

    [[nodiscard]] bool isCalculating() const { return m_isCalculating; }
    [[nodiscard]] bool hasRoute() const { return m_lastResult.success; }
    [[nodiscard]] double lastDistanceMeters() const { return m_lastResult.totalDistanceMeters; }
    [[nodiscard]] QString lastDistanceFormatted() const;
    [[nodiscard]] int lastDurationMinutes() const { return static_cast<int>(m_lastResult.estimatedDurationSeconds / 60); }
    [[nodiscard]] QString lastStatusMessage() const { return m_lastStatusMessage; }
    [[nodiscard]] QString lastPathSummary() const;
    [[nodiscard]] QVariantList lastCoordinates() const { return m_lastResult.pathCoordinates; }
    [[nodiscard]] QVariantList lastSteps() const { return m_lastResult.stepInstructions; }
    [[nodiscard]] double snappingToleranceMeters() const { return m_snappingToleranceMeters; }
    void setSnappingToleranceMeters(double meters);

    // Snapping
    std::optional<QString> snapToNearestNode(
        const QString &regionId,
        double lat, double lon,
        double maxToleranceMeters
    ) const;

    // Route calculation invokables
    Q_INVOKABLE bool calculateRoute(
        const QString &regionId,
        double startLat, double startLon,
        double destLat, double destLon,
        bool avoidHazards = true
    );

    Q_INVOKABLE bool calculateRouteByNodes(
        const QString &regionId,
        const QString &startNodeId,
        const QString &destNodeId,
        bool avoidHazards = true
    );

    Q_INVOKABLE void clearRoute();

    // Direct C++ evaluation
    RouteServiceResult computeRouteDirect(
        const QString &regionId,
        double startLat, double startLon,
        double destLat, double destLon,
        bool avoidHazards = true
    );

signals:
    void calculationStateChanged();
    void routeChanged();
    void statusMessageChanged();
    void snappingToleranceChanged();
    void routeComputed(bool success, double distanceMeters, const QString &message);

private:
    std::shared_ptr<IRouteRepository> m_repository;
    std::shared_ptr<MapPackageManager> m_packageManager;
    bool m_isCalculating{false};
    double m_snappingToleranceMeters{5000.0}; // 5.0 km default snapping tolerance
    QString m_lastStatusMessage;
    RouteServiceResult m_lastResult;

    void setStatus(const QString &msg);
};

} // namespace georescux::domain

#endif // GEORESCUX_ROUTE_SERVICE_H
