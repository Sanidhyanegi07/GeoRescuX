#ifndef GEORESCUX_QT_LOCATION_PROVIDER_H
#define GEORESCUX_QT_LOCATION_PROVIDER_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include "domain/ILocationProvider.h"

namespace georescux::platform {

/**
 * @brief Production implementation of ILocationProvider leveraging Qt 6 Core permissions
 * and system sensor intervals.
 */
class QtLocationProvider : public QObject, public georescux::domain::ILocationProvider {
    Q_OBJECT

public:
    explicit QtLocationProvider(QObject *parent = nullptr);
    ~QtLocationProvider() override;

    [[nodiscard]] georescux::domain::LocationPermissionStatus permissionStatus() const override;
    void requestPermission() override;

    [[nodiscard]] bool isLocationServicesEnabled() const override;
    void setLocationServicesEnabled(bool enabled);

    void startUpdates(bool highFrequencyEmergency = false) override;
    void stopUpdates() override;
    [[nodiscard]] bool isUpdating() const override;

    void setOnPositionChanged(std::function<void(const georescux::domain::GeoPosition&)> callback) override;
    void setOnStateChanged(std::function<void(georescux::domain::LocationTrackingState)> callback) override;
    void setOnPermissionChanged(std::function<void(georescux::domain::LocationPermissionStatus)> callback) override;

    // Optional desktop simulation / test harness position feed
    void setSimulatedPosition(double lat, double lon, double accuracyMeters = 5.0);

    // Hardware Android telemetry dispatch
    static QtLocationProvider* activeInstance() noexcept { return s_activeInstance; }
    void handleAndroidLocation(double lat, double lon, double alt, float acc, float speed, qint64 timestampUtcMs);
    void handleAndroidState(int stateCode);

private slots:
    void handlePollingTick();

private:
    void querySystemPermission();

    static inline QtLocationProvider* s_activeInstance{nullptr};

    georescux::domain::LocationPermissionStatus m_permission{georescux::domain::LocationPermissionStatus::NotRequested};
    bool m_servicesEnabled{true};
    bool m_isUpdating{false};
    bool m_isEmergencyFrequency{false};

    QTimer m_pollTimer;
    std::function<void(const georescux::domain::GeoPosition&)> m_onPositionChanged;
    std::function<void(georescux::domain::LocationTrackingState)> m_onStateChanged;
    std::function<void(georescux::domain::LocationPermissionStatus)> m_onPermissionChanged;

    bool m_hasSimulatedPos{false};
    double m_simLat{0.0};
    double m_simLon{0.0};
    double m_simAcc{0.0};
};

} // namespace georescux::platform

#endif // GEORESCUX_QT_LOCATION_PROVIDER_H
