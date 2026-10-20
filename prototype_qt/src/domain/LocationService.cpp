#include "LocationService.h"
#include <QDateTime>
#include <QDebug>
#include <cmath>

namespace georescux::domain {

namespace {
constexpr double kEarthRadiusMeters = 6371000.0;
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
}

LocationService::LocationService(
    std::shared_ptr<ILocationProvider> provider,
    std::shared_ptr<MapPackageManager> packageManager,
    std::function<qint64()> clockUtcMs,
    QObject *parent
) : QObject(parent),
    m_provider(std::move(provider)),
    m_packageManager(std::move(packageManager)),
    m_clock(std::move(clockUtcMs)) {

    if (!m_clock) {
        m_clock = []() -> qint64 {
            return QDateTime::currentMSecsSinceEpoch();
        };
    }

    if (m_provider) {
        m_provider->setOnPositionChanged([this](const GeoPosition &pos) {
            handlePositionUpdate(pos);
        });

        m_provider->setOnStateChanged([this](LocationTrackingState st) {
            handleProviderStateUpdate(st);
        });

        m_provider->setOnPermissionChanged([this](LocationPermissionStatus perm) {
            handleProviderPermissionUpdate(perm);
        });

        // Query initial state
        const auto perm = m_provider->permissionStatus();
        if (perm == LocationPermissionStatus::Denied) {
            m_trackingState = LocationTrackingState::PermissionDenied;
        } else if (perm == LocationPermissionStatus::NotRequested) {
            m_trackingState = LocationTrackingState::PermissionNotRequested;
        }
    }

    connect(&m_stalenessTimer, &QTimer::timeout, this, &LocationService::evaluateStaleness);
    m_stalenessTimer.setInterval(5000); // Check staleness every 5s
}

qint64 LocationService::currentTimeUtcMs() const {
    return m_clock ? m_clock() : QDateTime::currentMSecsSinceEpoch();
}

double LocationService::calculateDistanceMeters(double lat1, double lon1, double lat2, double lon2) const {
    const double dLat = (lat2 - lat1) * kDegToRad;
    const double dLon = (lon2 - lon1) * kDegToRad;
    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
                     std::cos(lat1 * kDegToRad) * std::cos(lat2 * kDegToRad) *
                     std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthRadiusMeters * c;
}

LocationPermissionStatus LocationService::permissionStatus() const noexcept {
    return m_provider ? m_provider->permissionStatus() : LocationPermissionStatus::NotRequested;
}

QString LocationService::permissionStatusName() const noexcept {
    return locationPermissionToString(permissionStatus());
}

bool LocationService::isTracking() const noexcept {
    return m_provider && m_provider->isUpdating();
}

QString LocationService::statusMessage() const noexcept {
    switch (m_trackingState) {
        case LocationTrackingState::Idle:
            return QStringLiteral("GPS Tracking Inactive");
        case LocationTrackingState::PermissionNotRequested:
            return QStringLiteral("Location Permission Not Requested");
        case LocationTrackingState::PermissionDenied:
            return QStringLiteral("Location Permission Denied by User");
        case LocationTrackingState::LocationServicesDisabled:
            return QStringLiteral("System Location Services Disabled");
        case LocationTrackingState::AcquisitionPending:
            return QStringLiteral("Acquiring GPS Satellite Fix...");
        case LocationTrackingState::ValidFixAcquired:
            if (m_isOutsideCoverage) {
                return QStringLiteral("GPS Locked (Outside Regional Map Coverage)");
            }
            return QStringLiteral("GPS Locked (Live Position Active)");
        case LocationTrackingState::StaleOrInaccurate:
            return QStringLiteral("GPS Degraded (Inaccurate or Stale Fix)");
        case LocationTrackingState::LocationUnavailable:
            return QStringLiteral("Location Service Unavailable");
        case LocationTrackingState::TemporarilyLost:
            return QStringLiteral("GPS Signal Lost (Displaying Last Known Fix)");
        default:
            return QStringLiteral("Unknown Location State");
    }
}

void LocationService::requestPermission() {
    if (m_provider) {
        m_provider->requestPermission();
    }
}

void LocationService::startTracking() {
    if (!m_provider) {
        return;
    }

    m_trackingStartTimeUtcMs = currentTimeUtcMs();
    m_provider->startUpdates(m_isEmergencyMode);
    m_stalenessTimer.start();

    if (m_provider->permissionStatus() == LocationPermissionStatus::Denied) {
        m_trackingState = LocationTrackingState::PermissionDenied;
        emit stateChanged(m_trackingState);
    } else if (!m_provider->isLocationServicesEnabled()) {
        m_trackingState = LocationTrackingState::LocationServicesDisabled;
        emit stateChanged(m_trackingState);
    } else {
        if (!m_lastValidPosition.has_value()) {
            m_trackingState = LocationTrackingState::AcquisitionPending;
            emit stateChanged(m_trackingState);
        }
    }
}

void LocationService::stopTracking() {
    m_stalenessTimer.stop();
    m_trackingStartTimeUtcMs = 0;
    if (m_provider) {
        m_provider->stopUpdates();
    }
    m_trackingState = LocationTrackingState::Idle;
    emit stateChanged(m_trackingState);
}

void LocationService::setEmergencyMode(bool active) {
    if (m_isEmergencyMode == active) {
        return;
    }

    m_isEmergencyMode = active;
    qInfo() << "[LocationService] Location tracking mode switched to:"
            << (m_isEmergencyMode ? "EMERGENCY (2s / 2m high frequency)" : "NORMAL (5s / 5m)");

    if (isTracking() && m_provider) {
        m_provider->startUpdates(m_isEmergencyMode);
    }
}

void LocationService::handlePositionUpdate(const GeoPosition &rawPosition) {
    // Step 1: Reject invalid coordinates (lat > 90, lon > 180, NaN, zeros)
    if (!rawPosition.isValid()) {
        qWarning() << "[LocationService] Rejected invalid raw coordinates: Lat:"
                   << rawPosition.latitude << "Lon:" << rawPosition.longitude;
        return;
    }

    m_latestPosition = rawPosition;

    // Step 2: Accuracy check
    if (rawPosition.accuracyMeters > m_policy.maxAcceptableAccuracyMeters) {
        qWarning() << "[LocationService] Position rejected due to high accuracy uncertainty: ±"
                   << rawPosition.accuracyMeters << "m (Threshold:" << m_policy.maxAcceptableAccuracyMeters << "m)";
        m_trackingState = LocationTrackingState::StaleOrInaccurate;
        emit stateChanged(m_trackingState);
        return;
    }

    // Step 3: Minimal displacement jitter filter
    if (m_lastValidPosition.has_value() && !m_isStale) {
        const double distanceMoved = calculateDistanceMeters(
            m_lastValidPosition->latitude, m_lastValidPosition->longitude,
            rawPosition.latitude, rawPosition.longitude
        );
        const double minDistance = m_isEmergencyMode
            ? m_policy.sosMinDistanceMeters
            : m_policy.normalMinDistanceMeters;

        // Skip jitter update if displacement is below threshold and fix is recent (< 2s)
        const qint64 timeDelta = rawPosition.timestampUtcMs - m_lastValidPosition->timestampUtcMs;
        if (distanceMoved < minDistance && timeDelta < 2000) {
            return;
        }
    }

    // Step 4: Coverage bounding check against active region
    updateCoverageStatus(rawPosition.latitude, rawPosition.longitude);

    // Step 5: Accept fix
    m_lastValidPosition = rawPosition;
    m_isStale = false;
    m_trackingState = LocationTrackingState::ValidFixAcquired;

    emit positionChanged(m_lastValidPosition.value());
    emit stateChanged(m_trackingState);
}

void LocationService::updateCoverageStatus(double lat, double lon) {
    if (!m_packageManager || !m_packageManager->hasActivePackage()) {
        if (m_isOutsideCoverage) {
            m_isOutsideCoverage = false;
            emit coverageChanged(false);
        }
        return;
    }

    const bool isOutside = !m_packageManager->isCoordinateCovered(lat, lon);

    if (m_isOutsideCoverage != isOutside) {
        m_isOutsideCoverage = isOutside;
        emit coverageChanged(m_isOutsideCoverage);
        if (m_isOutsideCoverage) {
            qWarning() << "[LocationService] Current location is outside coverage of active region:"
                       << m_packageManager->activeRegionId()
                       << "| Lat:" << lat << "Lon:" << lon;
        }
    }
}

void LocationService::handleProviderStateUpdate(LocationTrackingState providerState) {
    if (providerState == LocationTrackingState::TemporarilyLost ||
        providerState == LocationTrackingState::LocationUnavailable) {
        m_isStale = true;
        m_trackingState = providerState;
        emit stateChanged(m_trackingState);
    } else if (providerState == LocationTrackingState::PermissionDenied) {
        m_trackingState = LocationTrackingState::PermissionDenied;
        emit stateChanged(m_trackingState);
    } else if (providerState == LocationTrackingState::LocationServicesDisabled) {
        m_trackingState = LocationTrackingState::LocationServicesDisabled;
        emit stateChanged(m_trackingState);
    }
}

void LocationService::handleProviderPermissionUpdate(LocationPermissionStatus perm) {
    emit permissionChanged(perm);
    if (perm == LocationPermissionStatus::Denied) {
        m_trackingState = LocationTrackingState::PermissionDenied;
        emit stateChanged(m_trackingState);
    } else if (perm == LocationPermissionStatus::Granted) {
        if (isTracking()) {
            m_trackingState = LocationTrackingState::AcquisitionPending;
            emit stateChanged(m_trackingState);
        }
    }
}

void LocationService::evaluateStaleness() {
    if (!isTracking()) {
        return;
    }

    if (m_lastValidPosition.has_value()) {
        const qint64 elapsedMs = currentTimeUtcMs() - m_lastValidPosition->timestampUtcMs;
        if (elapsedMs >= m_policy.staleTimeoutMs) {
            if (!m_isStale) {
                m_isStale = true;
                m_trackingState = LocationTrackingState::StaleOrInaccurate;
                qWarning() << "[LocationService] GPS fix became stale (No fresh fix for" << (elapsedMs / 1000) << "s). Preserving last known location.";
                emit stateChanged(m_trackingState);
            }
        }
    } else {
        // Initial acquisition timeout: if acquisition has been pending for >= staleTimeoutMs
        if (m_trackingStartTimeUtcMs > 0 &&
            m_trackingState == LocationTrackingState::AcquisitionPending) {
            const qint64 pendingMs = currentTimeUtcMs() - m_trackingStartTimeUtcMs;
            if (pendingMs >= m_policy.staleTimeoutMs) {
                m_trackingState = LocationTrackingState::LocationUnavailable;
                qWarning() << "[LocationService] Initial GPS satellite acquisition timed out after"
                           << (pendingMs / 1000) << "s. Location unavailable.";
                emit stateChanged(m_trackingState);
            }
        }
    }
}

} // namespace georescux::domain
