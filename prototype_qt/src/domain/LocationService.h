#ifndef GEORESCUX_LOCATION_SERVICE_H
#define GEORESCUX_LOCATION_SERVICE_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>
#include <optional>
#include <functional>
#include "LocationModel.h"
#include "ILocationProvider.h"
#include "MapPackageManager.h"

namespace georescux::domain {

/**
 * @brief Authoritative domain coordinator for device location acquisition,
 * permission management, freshness tracking, and geographic boundary validation.
 */
class LocationService : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasValidFix READ hasValidFix NOTIFY positionChanged)
    Q_PROPERTY(double currentLatitude READ currentLatitude NOTIFY positionChanged)
    Q_PROPERTY(double currentLongitude READ currentLongitude NOTIFY positionChanged)
    Q_PROPERTY(double currentAccuracyMeters READ currentAccuracyMeters NOTIFY positionChanged)
    Q_PROPERTY(QString trackingStateName READ trackingStateName NOTIFY stateChanged)
    Q_PROPERTY(QString permissionStatusName READ permissionStatusName NOTIFY permissionChanged)
    Q_PROPERTY(bool isStale READ isStale NOTIFY stateChanged)
    Q_PROPERTY(bool isOutsideCoverage READ isOutsideCoverage NOTIFY coverageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(bool isTracking READ isTracking NOTIFY stateChanged)

public:
    explicit LocationService(
        std::shared_ptr<ILocationProvider> provider,
        std::shared_ptr<MapPackageManager> packageManager = nullptr,
        std::function<qint64()> clockUtcMs = nullptr,
        QObject *parent = nullptr
    );
    ~LocationService() override = default;

    [[nodiscard]] bool hasValidFix() const noexcept { return m_lastValidPosition.has_value() && !m_isStale; }
    [[nodiscard]] double currentLatitude() const noexcept { return m_lastValidPosition ? m_lastValidPosition->latitude : 0.0; }
    [[nodiscard]] double currentLongitude() const noexcept { return m_lastValidPosition ? m_lastValidPosition->longitude : 0.0; }
    [[nodiscard]] double currentAccuracyMeters() const noexcept { return m_lastValidPosition ? m_lastValidPosition->accuracyMeters : 0.0; }
    [[nodiscard]] QString trackingStateName() const noexcept { return locationTrackingStateToString(m_trackingState); }
    [[nodiscard]] QString permissionStatusName() const noexcept;
    [[nodiscard]] bool isStale() const noexcept { return m_isStale; }
    [[nodiscard]] bool isOutsideCoverage() const noexcept { return m_isOutsideCoverage; }
    [[nodiscard]] QString statusMessage() const noexcept;
    [[nodiscard]] bool isTracking() const noexcept;

    [[nodiscard]] LocationTrackingState trackingState() const noexcept { return m_trackingState; }
    [[nodiscard]] LocationPermissionStatus permissionStatus() const noexcept;
    [[nodiscard]] const std::optional<GeoPosition> &latestPosition() const noexcept { return m_latestPosition; }
    [[nodiscard]] const std::optional<GeoPosition> &lastValidPosition() const noexcept { return m_lastValidPosition; }

    [[nodiscard]] const LocationUpdatePolicy &policy() const noexcept { return m_policy; }
    void setPolicy(const LocationUpdatePolicy &policy) { m_policy = policy; }

    // Control operations
    Q_INVOKABLE void requestPermission();
    Q_INVOKABLE void startTracking();
    Q_INVOKABLE void stopTracking();
    void setEmergencyMode(bool active);

    // Staleness & lifecycle evaluation
    void evaluateStaleness();

signals:
    void positionChanged(const georescux::domain::GeoPosition &position);
    void stateChanged(georescux::domain::LocationTrackingState newState);
    void permissionChanged(georescux::domain::LocationPermissionStatus newPermission);
    void coverageChanged(bool isOutside);

private:
    void handlePositionUpdate(const GeoPosition &rawPosition);
    void handleProviderStateUpdate(LocationTrackingState providerState);
    void handleProviderPermissionUpdate(LocationPermissionStatus permission);
    void updateCoverageStatus(double lat, double lon);
    [[nodiscard]] qint64 currentTimeUtcMs() const;
    [[nodiscard]] double calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) const;

    std::shared_ptr<ILocationProvider> m_provider;
    std::shared_ptr<MapPackageManager> m_packageManager;
    std::function<qint64()> m_clock;

    LocationUpdatePolicy m_policy;
    LocationTrackingState m_trackingState{LocationTrackingState::Idle};
    std::optional<GeoPosition> m_latestPosition{std::nullopt};
    std::optional<GeoPosition> m_lastValidPosition{std::nullopt};
    bool m_isStale{false};
    bool m_isOutsideCoverage{false};
    bool m_isEmergencyMode{false};
    qint64 m_trackingStartTimeUtcMs{0};

    QTimer m_stalenessTimer;
};

} // namespace georescux::domain

#endif // GEORESCUX_LOCATION_SERVICE_H
