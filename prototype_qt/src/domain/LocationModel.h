#ifndef GEORESCUX_LOCATION_MODEL_H
#define GEORESCUX_LOCATION_MODEL_H

#include <QString>
#include <QtGlobal>
#include <cmath>

namespace georescux::domain {

/**
 * @brief Android / system runtime location permission state.
 */
enum class LocationPermissionStatus {
    NotRequested = 0,
    Granted = 1,
    Denied = 2
};

/**
 * @brief Discrete state machine of the location tracking subsystem.
 */
enum class LocationTrackingState {
    Idle = 0,
    PermissionNotRequested = 1,
    PermissionDenied = 2,
    LocationServicesDisabled = 3,
    AcquisitionPending = 4,
    ValidFixAcquired = 5,
    StaleOrInaccurate = 6,
    LocationUnavailable = 7,
    TemporarilyLost = 8
};

/**
 * @brief Raw geographic position snapshot with accuracy and timing attributes.
 */
struct GeoPosition {
    double latitude{0.0};
    double longitude{0.0};
    double altitudeMeters{0.0};
    double accuracyMeters{0.0};
    double speedMps{0.0};
    qint64 timestampUtcMs{0};
    bool isValidCoordinates{false};

    [[nodiscard]] bool isValid() const noexcept {
        return isValidCoordinates &&
               std::isfinite(latitude) && std::isfinite(longitude) &&
               (latitude != 0.0 || longitude != 0.0) &&
               (latitude >= -90.0 && latitude <= 90.0) &&
               (longitude >= -180.0 && longitude <= 180.0);
    }
};

/**
 * @brief Configuration parameters regulating GPS polling frequency, accuracy, and staleness.
 */
struct LocationUpdatePolicy {
    // Standard map tracking thresholds (preserves battery & storage)
    int normalIntervalMs{5000};            // 5-second polling interval
    double normalMinDistanceMeters{5.0};   // 5 meters minimum displacement

    // Life-Safety Emergency SOS thresholds (high-urgency evacuation tracking)
    int sosIntervalMs{2000};               // 2-second polling interval
    double sosMinDistanceMeters{2.0};      // 2 meters minimum displacement

    // Accuracy and freshness gating
    double maxAcceptableAccuracyMeters{60.0}; // Above 60m rejected as inaccurate
    int staleTimeoutMs{30000};                // 30 seconds without fresh fix = stale/lost
};

inline QString locationPermissionToString(LocationPermissionStatus status) {
    switch (status) {
        case LocationPermissionStatus::Granted:      return QStringLiteral("GRANTED");
        case LocationPermissionStatus::Denied:       return QStringLiteral("DENIED");
        case LocationPermissionStatus::NotRequested:
        default:                                     return QStringLiteral("NOT_REQUESTED");
    }
}

inline QString locationTrackingStateToString(LocationTrackingState state) {
    switch (state) {
        case LocationTrackingState::Idle:                     return QStringLiteral("IDLE");
        case LocationTrackingState::PermissionNotRequested:   return QStringLiteral("PERMISSION_NOT_REQUESTED");
        case LocationTrackingState::PermissionDenied:          return QStringLiteral("PERMISSION_DENIED");
        case LocationTrackingState::LocationServicesDisabled: return QStringLiteral("SERVICES_DISABLED");
        case LocationTrackingState::AcquisitionPending:        return QStringLiteral("ACQUIRING_FIX");
        case LocationTrackingState::ValidFixAcquired:          return QStringLiteral("VALID_FIX");
        case LocationTrackingState::StaleOrInaccurate:         return QStringLiteral("STALE_OR_INACCURATE");
        case LocationTrackingState::LocationUnavailable:       return QStringLiteral("UNAVAILABLE");
        case LocationTrackingState::TemporarilyLost:          return QStringLiteral("TEMPORARILY_LOST");
        default:                                              return QStringLiteral("UNKNOWN");
    }
}

} // namespace georescux::domain

#endif // GEORESCUX_LOCATION_MODEL_H
