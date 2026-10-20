#ifndef GEORESCUX_EMERGENCY_MODEL_H
#define GEORESCUX_EMERGENCY_MODEL_H

#include <QString>
#include <QDateTime>
#include <optional>
#include <vector>

namespace georescux::domain {

/**
 * @brief Authoritative lifecycle status of a single emergency event.
 */
enum class EmergencyStatus {
    Active = 0,
    Completed = 1
};

/**
 * @brief Categorization of location availability for an emergency.
 */
enum class EmergencyLocationStatus {
    Unknown = 0,
    Available = 1,
    PermissionDenied = 2,
    Unavailable = 3
};

/**
 * @brief Local synchronization state of the emergency record.
 */
enum class EmergencySyncStatus {
    Pending = 0,
    Synced = 1,
    Failed = 2
};

/**
 * @brief Result and error codes for emergency operations.
 */
enum class EmergencyErrorCode {
    Success = 0,
    AlreadyActive = 1,
    NotFound = 2,
    InvalidInput = 3,
    StorageUnavailable = 4,
    StorageFailure = 5,
    InvalidState = 6
};

/**
 * @brief Single coordinate fix snapshot attached to an emergency event.
 */
struct EmergencyLocation {
    double latitude{0.0};
    double longitude{0.0};
    double accuracyMeters{0.0};
    qint64 timestampUtcMs{0};

    [[nodiscard]] bool isValid() const noexcept {
        return (latitude != 0.0 || longitude != 0.0) &&
               (latitude >= -90.0 && latitude <= 90.0) &&
               (longitude >= -180.0 && longitude <= 180.0);
    }
};

/**
 * @brief Typed, immutable domain model of an emergency record.
 */
struct EmergencyRecord {
    QString emergencyId;
    QString userId;
    EmergencyStatus status{EmergencyStatus::Active};
    qint64 startedAtUtcMs{0};
    std::optional<qint64> completedAtUtcMs{std::nullopt};
    std::optional<EmergencyLocation> location{std::nullopt};
    EmergencyLocationStatus locationStatus{EmergencyLocationStatus::Unknown};
    QString note;
    qint64 createdAtUtcMs{0};
    qint64 updatedAtUtcMs{0};
    EmergencySyncStatus syncStatus{EmergencySyncStatus::Pending};
    int schemaVersion{2};

    [[nodiscard]] bool isActive() const noexcept {
        return status == EmergencyStatus::Active && !completedAtUtcMs.has_value();
    }

    [[nodiscard]] bool isCompleted() const noexcept {
        return status == EmergencyStatus::Completed && completedAtUtcMs.has_value();
    }

    [[nodiscard]] bool isValid() const noexcept {
        return !emergencyId.trimmed().isEmpty() &&
               !userId.trimmed().isEmpty() &&
               startedAtUtcMs > 0;
    }
};

/**
 * @brief Structured result wrapper for emergency domain operations.
 */
template <typename T>
struct EmergencyResult {
    bool isSuccess{false};
    EmergencyErrorCode errorCode{EmergencyErrorCode::Success};
    QString errorMessage;
    std::optional<T> data{std::nullopt};

    static EmergencyResult<T> ok(T value) {
        EmergencyResult<T> res;
        res.isSuccess = true;
        res.errorCode = EmergencyErrorCode::Success;
        res.data = std::move(value);
        return res;
    }

    static EmergencyResult<T> fail(EmergencyErrorCode code, const QString &message) {
        EmergencyResult<T> res;
        res.isSuccess = false;
        res.errorCode = code;
        res.errorMessage = message;
        return res;
    }
};

/**
 * @brief Specialization of EmergencyResult for operations without return data.
 */
template <>
struct EmergencyResult<void> {
    bool isSuccess{false};
    EmergencyErrorCode errorCode{EmergencyErrorCode::Success};
    QString errorMessage;

    static EmergencyResult<void> ok() {
        EmergencyResult<void> res;
        res.isSuccess = true;
        res.errorCode = EmergencyErrorCode::Success;
        return res;
    }

    static EmergencyResult<void> fail(EmergencyErrorCode code, const QString &message) {
        EmergencyResult<void> res;
        res.isSuccess = false;
        res.errorCode = code;
        res.errorMessage = message;
        return res;
    }
};

using EmergencyResultVoid = EmergencyResult<void>;

// Domain Utility Functions
QString emergencyStatusToString(EmergencyStatus status);
EmergencyStatus emergencyStatusFromString(const QString &str);

QString locationStatusToString(EmergencyLocationStatus status);
EmergencyLocationStatus locationStatusFromString(const QString &str);

QString syncStatusToString(EmergencySyncStatus status);
EmergencySyncStatus syncStatusFromString(const QString &str);

QString emergencyErrorToString(EmergencyErrorCode code);

} // namespace georescux::domain

#endif // GEORESCUX_EMERGENCY_MODEL_H
