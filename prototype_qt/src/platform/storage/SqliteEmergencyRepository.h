#ifndef GEORESCUX_SQLITE_EMERGENCY_REPOSITORY_H
#define GEORESCUX_SQLITE_EMERGENCY_REPOSITORY_H

#include <memory>
#include "domain/IEmergencyRepository.h"
#include "LocalDatabase.h"

namespace georescux::storage {

/**
 * @brief Thread-safe SQLite implementation of IEmergencyRepository.
 *
 * Enforces single-active-emergency invariant per user via SQLite partial
 * unique index and atomic transactions. Provides strict user isolation.
 */
class SqliteEmergencyRepository : public georescux::domain::IEmergencyRepository {
public:
    explicit SqliteEmergencyRepository(std::shared_ptr<LocalDatabase> database);
    ~SqliteEmergencyRepository() override = default;

    georescux::domain::EmergencyResult<std::optional<georescux::domain::EmergencyRecord>>
    getActiveEmergency(const QString &userId) override;

    georescux::domain::EmergencyResult<georescux::domain::EmergencyRecord>
    activateEmergency(const QString &userId, const QString &note = QString()) override;

    georescux::domain::EmergencyResult<georescux::domain::EmergencyRecord>
    updateLocation(
        const QString &userId,
        const QString &emergencyId,
        const georescux::domain::EmergencyLocation &location,
        georescux::domain::EmergencyLocationStatus status
    ) override;

    georescux::domain::EmergencyResult<georescux::domain::EmergencyRecord>
    updateNote(const QString &userId, const QString &emergencyId, const QString &note) override;

    georescux::domain::EmergencyResult<georescux::domain::EmergencyRecord>
    completeEmergency(const QString &userId, const QString &emergencyId) override;

    georescux::domain::EmergencyResult<std::vector<georescux::domain::EmergencyRecord>>
    getEmergencyHistory(const QString &userId, int limit = 20) override;

private:
    georescux::domain::EmergencyRecord recordFromQuery(class QSqlQuery &q) const;

    std::shared_ptr<LocalDatabase> m_database;
};

} // namespace georescux::storage

#endif // GEORESCUX_SQLITE_EMERGENCY_REPOSITORY_H
