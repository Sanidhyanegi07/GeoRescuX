#include "EmergencyModel.h"

namespace georescux::domain {

QString emergencyStatusToString(EmergencyStatus status) {
    switch (status) {
        case EmergencyStatus::Active:    return QStringLiteral("ACTIVE");
        case EmergencyStatus::Completed: return QStringLiteral("COMPLETED");
    }
    return QStringLiteral("UNKNOWN");
}

EmergencyStatus emergencyStatusFromString(const QString &str) {
    if (str.compare(QStringLiteral("COMPLETED"), Qt::CaseInsensitive) == 0) {
        return EmergencyStatus::Completed;
    }
    return EmergencyStatus::Active;
}

QString locationStatusToString(EmergencyLocationStatus status) {
    switch (status) {
        case EmergencyLocationStatus::Unknown:          return QStringLiteral("UNKNOWN");
        case EmergencyLocationStatus::Available:        return QStringLiteral("AVAILABLE");
        case EmergencyLocationStatus::PermissionDenied: return QStringLiteral("PERMISSION_DENIED");
        case EmergencyLocationStatus::Unavailable:      return QStringLiteral("UNAVAILABLE");
    }
    return QStringLiteral("UNKNOWN");
}

EmergencyLocationStatus locationStatusFromString(const QString &str) {
    if (str.compare(QStringLiteral("AVAILABLE"), Qt::CaseInsensitive) == 0) {
        return EmergencyLocationStatus::Available;
    }
    if (str.compare(QStringLiteral("PERMISSION_DENIED"), Qt::CaseInsensitive) == 0) {
        return EmergencyLocationStatus::PermissionDenied;
    }
    if (str.compare(QStringLiteral("UNAVAILABLE"), Qt::CaseInsensitive) == 0) {
        return EmergencyLocationStatus::Unavailable;
    }
    return EmergencyLocationStatus::Unknown;
}

QString syncStatusToString(EmergencySyncStatus status) {
    switch (status) {
        case EmergencySyncStatus::Pending: return QStringLiteral("PENDING");
        case EmergencySyncStatus::Synced:  return QStringLiteral("SYNCED");
        case EmergencySyncStatus::Failed:  return QStringLiteral("FAILED");
    }
    return QStringLiteral("PENDING");
}

EmergencySyncStatus syncStatusFromString(const QString &str) {
    if (str.compare(QStringLiteral("SYNCED"), Qt::CaseInsensitive) == 0) {
        return EmergencySyncStatus::Synced;
    }
    if (str.compare(QStringLiteral("FAILED"), Qt::CaseInsensitive) == 0) {
        return EmergencySyncStatus::Failed;
    }
    return EmergencySyncStatus::Pending;
}

QString emergencyErrorToString(EmergencyErrorCode code) {
    switch (code) {
        case EmergencyErrorCode::Success:            return QStringLiteral("Success");
        case EmergencyErrorCode::AlreadyActive:       return QStringLiteral("An emergency is already active for this user");
        case EmergencyErrorCode::NotFound:            return QStringLiteral("Emergency record not found");
        case EmergencyErrorCode::InvalidInput:        return QStringLiteral("Invalid input parameter");
        case EmergencyErrorCode::StorageUnavailable: return QStringLiteral("Local persistence storage is unavailable");
        case EmergencyErrorCode::StorageFailure:     return QStringLiteral("Database operation failed");
        case EmergencyErrorCode::InvalidState:        return QStringLiteral("Operation invalid in current emergency state");
    }
    return QStringLiteral("Unknown error");
}

} // namespace georescux::domain
