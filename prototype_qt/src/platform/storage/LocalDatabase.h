#ifndef GEORESCUX_LOCAL_DATABASE_H
#define GEORESCUX_LOCAL_DATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <QRecursiveMutex>
#include <optional>
#include "domain/SessionModel.h"

class QThread;

namespace georescux::storage {

/**
 * @brief Local SQLite persistence service managing schema migrations,
 * user preferences, and session state caching.
 *
 * Threading Model & Security Notes:
 * - Per Qt SQL documentation, QSqlDatabase connections are thread-affine and
 *   must be used exclusively from the thread in which they were created.
 * - LocalDatabase records its creator thread (m_creatorThread) upon initialize()
 *   and guards against cross-thread access.
 * - Re-entrant calls on the creator thread are synchronized with m_mutex.
 */
class LocalDatabase {
public:
    static constexpr int CURRENT_SCHEMA_VERSION = 1;

    LocalDatabase();
    ~LocalDatabase();

    // Non-copyable
    LocalDatabase(const LocalDatabase &) = delete;
    LocalDatabase &operator=(const LocalDatabase &) = delete;

    /**
     * @brief Initializes the database connection and runs pending schema migrations.
     * @param databasePath Path to SQLite file, or ":memory:" for deterministic in-memory tests.
     * If empty, defaults to QStandardPaths::AppDataLocation/georescux_prototype.db.
     */
    bool initialize(const QString &databasePath = QString());

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_isInitialized; }
    [[nodiscard]] int schemaVersion() const;
    [[nodiscard]] QString databasePath() const noexcept { return m_dbPath; }
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }

    // Key-Value App Preferences
    bool setPreference(const QString &key, const QString &value);
    [[nodiscard]] QString getPreference(const QString &key, const QString &defaultValue = QString()) const;

    // Session Caching (without storing plaintext passwords/tokens)
    bool saveSession(const georescux::domain::UserProfile &profile);
    [[nodiscard]] std::optional<georescux::domain::UserProfile> loadSession() const;
    bool clearSession();

    // Scaffolding for Emergency State (Phase 3 schema preparation)
    bool recordEmergencyScaffold(
        const QString &emergencyId,
        const QString &status,
        qint64 startedAtMs,
        double lat,
        double lon
    );

    void close();

private:
    bool applyMigrations();
    bool executeSchemaV1();
    bool checkThreadAffinity() const;

    QString m_connectionName;
    QString m_dbPath;
    mutable QString m_lastError;
    mutable QRecursiveMutex m_mutex;
    QThread *m_creatorThread{nullptr};
    bool m_isInitialized{false};
};

} // namespace georescux::storage

#endif // GEORESCUX_LOCAL_DATABASE_H
