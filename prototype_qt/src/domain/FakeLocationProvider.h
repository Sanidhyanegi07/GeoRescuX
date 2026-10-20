#ifndef GEORESCUX_FAKE_LOCATION_PROVIDER_H
#define GEORESCUX_FAKE_LOCATION_PROVIDER_H

#include "ILocationProvider.h"

namespace georescux::domain {

/**
 * @brief Deterministic, programmable fake location provider for unit testing.
 * Exposes precise hooks to simulate GPS signals, satellite lock delays, permission denials, and dropouts.
 */
class FakeLocationProvider : public ILocationProvider {
public:
    explicit FakeLocationProvider(LocationPermissionStatus initialPermission = LocationPermissionStatus::Granted)
        : m_permission(initialPermission) {}
    ~FakeLocationProvider() override = default;

    [[nodiscard]] LocationPermissionStatus permissionStatus() const override {
        return m_permission;
    }

    void setPermissionStatus(LocationPermissionStatus status) {
        m_permission = status;
        if (m_onPermissionChanged) {
            m_onPermissionChanged(m_permission);
        }
        if (m_permission == LocationPermissionStatus::Denied) {
            if (m_onStateChanged) {
                m_onStateChanged(LocationTrackingState::PermissionDenied);
            }
        }
    }

    void requestPermission() override {
        // If not requested, auto-grant or keep current for test control
        if (m_permission == LocationPermissionStatus::NotRequested) {
            m_permission = LocationPermissionStatus::Granted;
            if (m_onPermissionChanged) {
                m_onPermissionChanged(m_permission);
            }
        }
    }

    [[nodiscard]] bool isLocationServicesEnabled() const override {
        return m_servicesEnabled;
    }

    void setLocationServicesEnabled(bool enabled) {
        m_servicesEnabled = enabled;
        if (!m_servicesEnabled && m_onStateChanged) {
            m_onStateChanged(LocationTrackingState::LocationServicesDisabled);
        }
    }

    void startUpdates(bool highFrequencyEmergency = false) override {
        m_isUpdating = true;
        m_isEmergencyFrequency = highFrequencyEmergency;
        m_startCount++;

        if (m_permission == LocationPermissionStatus::Denied) {
            if (m_onStateChanged) m_onStateChanged(LocationTrackingState::PermissionDenied);
            return;
        }
        if (!m_servicesEnabled) {
            if (m_onStateChanged) m_onStateChanged(LocationTrackingState::LocationServicesDisabled);
            return;
        }

        if (m_onStateChanged) {
            m_onStateChanged(LocationTrackingState::AcquisitionPending);
        }
    }

    void stopUpdates() override {
        m_isUpdating = false;
        m_isEmergencyFrequency = false;
        m_stopCount++;
        if (m_onStateChanged) {
            m_onStateChanged(LocationTrackingState::Idle);
        }
    }

    [[nodiscard]] bool isUpdating() const override {
        return m_isUpdating;
    }

    [[nodiscard]] bool isEmergencyFrequency() const noexcept {
        return m_isEmergencyFrequency;
    }

    [[nodiscard]] int startCount() const noexcept { return m_startCount; }
    [[nodiscard]] int stopCount() const noexcept { return m_stopCount; }

    void setOnPositionChanged(std::function<void(const GeoPosition&)> callback) override {
        m_onPositionChanged = std::move(callback);
    }

    void setOnStateChanged(std::function<void(LocationTrackingState)> callback) override {
        m_onStateChanged = std::move(callback);
    }

    void setOnPermissionChanged(std::function<void(LocationPermissionStatus)> callback) override {
        m_onPermissionChanged = std::move(callback);
    }

    // --- Test Control Methods ---
    void injectFix(
        double lat,
        double lon,
        double accuracyMeters,
        qint64 timestampUtcMs,
        double speedMps = 0.0,
        double altitudeMeters = 0.0
    ) {
        GeoPosition pos{
            .latitude = lat,
            .longitude = lon,
            .altitudeMeters = altitudeMeters,
            .accuracyMeters = accuracyMeters,
            .speedMps = speedMps,
            .timestampUtcMs = timestampUtcMs,
            .isValidCoordinates = true
        };

        if (m_onPositionChanged) {
            m_onPositionChanged(pos);
        }
    }

    void simulateSignalLoss() {
        if (m_onStateChanged) {
            m_onStateChanged(LocationTrackingState::TemporarilyLost);
        }
    }

    void simulateUnavailable() {
        if (m_onStateChanged) {
            m_onStateChanged(LocationTrackingState::LocationUnavailable);
        }
    }

private:
    LocationPermissionStatus m_permission{LocationPermissionStatus::Granted};
    bool m_servicesEnabled{true};
    bool m_isUpdating{false};
    bool m_isEmergencyFrequency{false};
    int m_startCount{0};
    int m_stopCount{0};

    std::function<void(const GeoPosition&)> m_onPositionChanged;
    std::function<void(LocationTrackingState)> m_onStateChanged;
    std::function<void(LocationPermissionStatus)> m_onPermissionChanged;
};

} // namespace georescux::domain

#endif // GEORESCUX_FAKE_LOCATION_PROVIDER_H
