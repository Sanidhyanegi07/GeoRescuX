#ifndef GEORESCUX_I_EMERGENCY_REPOSITORY_H
#define GEORESCUX_I_EMERGENCY_REPOSITORY_H

#include <QString>
#include <optional>
#include <vector>
#include "EmergencyModel.h"

namespace georescux::domain {

/**
 * @brief Interface contract for emergency state persistence and retrieval.
 *
 * Enforces transactional single-active-emergency invariant and strict user isolation.
 */
class IEmergencyRepository {
public:
    virtual ~IEmergencyRepository() = default;

    /**
     * @brief Retrieves the single active emergency for the given user, if any.
     */
    virtual EmergencyResult<std::optional<EmergencyRecord>> getActiveEmergency(const QString &userId) = 0;

    /**
     * @brief Activates a new emergency for the given user.
     * Enforces the invariant that a user can have at most one Active emergency.
     * If an active emergency already exists, returns EmergencyErrorCode::AlreadyActive
     * containing the existing active emergency record.
     */
    virtual EmergencyResult<EmergencyRecord> activateEmergency(
        const QString &userId,
        const QString &note = QString()
    ) = 0;

    /**
     * @brief Updates the location of an existing emergency record.
     * Enforces user ownership: caller must match user_id on active record.
     */
    virtual EmergencyResult<EmergencyRecord> updateLocation(
        const QString &userId,
        const QString &emergencyId,
        const EmergencyLocation &location,
        EmergencyLocationStatus status
    ) = 0;

    /**
     * @brief Updates the optional emergency note (capped at 120 characters).
     * Enforces user ownership: caller must match user_id on active record.
     */
    virtual EmergencyResult<EmergencyRecord> updateNote(
        const QString &userId,
        const QString &emergencyId,
        const QString &note
    ) = 0;

    /**
     * @brief Completes an active emergency, recording completedAt timestamp
     * and moving it into immutable history.
     */
    virtual EmergencyResult<EmergencyRecord> completeEmergency(
        const QString &userId,
        const QString &emergencyId
    ) = 0;

    /**
     * @brief Retrieves finished emergency records for the user in reverse chronological order.
     */
    virtual EmergencyResult<std::vector<EmergencyRecord>> getEmergencyHistory(
        const QString &userId,
        int limit = 20
    ) = 0;
};

} // namespace georescux::domain

#endif // GEORESCUX_I_EMERGENCY_REPOSITORY_H
