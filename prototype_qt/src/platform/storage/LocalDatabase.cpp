#include "LocalDatabase.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QDateTime>
#include <QThread>
#include <QDebug>

namespace georescux::storage {

LocalDatabase::LocalDatabase()
    : m_connectionName(QStringLiteral("georescux_db_") + QUuid::createUuid().toString(QUuid::Id128))
{
}

LocalDatabase::~LocalDatabase() {
    close();
}

bool LocalDatabase::checkThreadAffinity() const {
    if (m_creatorThread && QThread::currentThread() != m_creatorThread) {
        m_lastError = QStringLiteral("Threading violation: QSqlDatabase connection '") +
                      m_connectionName +
                      QStringLiteral("' accessed from non-creator thread. Qt SQL connections must not be shared across threads.");
        qCritical().noquote() << "[LocalDatabase]" << m_lastError;
        return false;
    }
    return true;
}

bool LocalDatabase::initialize(const QString &databasePath) {
    QMutexLocker locker(&m_mutex);

    m_creatorThread = QThread::currentThread();

    if (m_isInitialized && QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) {
            return true;
        }
    }

    if (databasePath.isEmpty()) {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(appDataDir);
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        m_dbPath = dir.filePath(QStringLiteral("georescux_prototype.db"));
    } else {
        m_dbPath = databasePath;
    }

    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connectionName)) {
        db = QSqlDatabase::database(m_connectionName);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    }

    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        m_lastError = QStringLiteral("Failed to open SQLite database: ") + db.lastError().text();
        qCritical().noquote() << "[LocalDatabase]" << m_lastError;
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_isInitialized = false;
        return false;
    }

    if (!applyMigrations()) {
        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_isInitialized = false;
        return false;
    }

    m_isInitialized = true;
    m_lastError.clear();
    qInfo().noquote() << "[LocalDatabase] Initialized successfully at:" << m_dbPath
                      << "| Schema version:" << schemaVersion();
    return true;
}

bool LocalDatabase::isOpen() const noexcept {
    QMutexLocker locker(&m_mutex);
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName).isOpen();
}

int LocalDatabase::schemaVersion() const {
    QMutexLocker locker(&m_mutex);
    if (!QSqlDatabase::contains(m_connectionName)) {
        return 0;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_migrations;"))) {
        return 0;
    }

    if (query.next() && !query.isNull(0)) {
        return query.value(0).toInt();
    }
    return 0;
}

bool LocalDatabase::applyMigrations() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    // Initial table for schema tracking
    QSqlQuery initQuery(db);
    const QString createMigrationsTable = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version INTEGER PRIMARY KEY,"
        "  applied_at_ms INTEGER NOT NULL,"
        "  description TEXT NOT NULL"
        ");"
    );

    if (!initQuery.exec(createMigrationsTable)) {
        m_lastError = QStringLiteral("Failed to create schema_migrations table: ") + initQuery.lastError().text();
        return false;
    }

    int currentVer = schemaVersion();
    if (currentVer < 1) {
        if (!executeSchemaV1()) {
            return false;
        }
        currentVer = schemaVersion();
    }

    if (currentVer < 2) {
        if (!executeSchemaV2()) {
            return false;
        }
    }

    return true;
}

bool LocalDatabase::executeSchemaV1() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Failed to begin transaction for Schema V1.");
        return false;
    }

    QSqlQuery q(db);

    // 1. App preferences key-value store
    const QString createPrefs = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS app_preferences ("
        "  pref_key TEXT PRIMARY KEY,"
        "  pref_value TEXT NOT NULL,"
        "  updated_at_ms INTEGER NOT NULL"
        ");"
    );
    if (!q.exec(createPrefs)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create app_preferences table: ") + q.lastError().text();
        return false;
    }

    // 2. Cached session metadata (zero plaintext credentials)
    const QString createSessions = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS cached_session ("
        "  session_id TEXT PRIMARY KEY,"
        "  uid TEXT NOT NULL,"
        "  display_name TEXT NOT NULL,"
        "  email TEXT NOT NULL,"
        "  phone TEXT NOT NULL,"
        "  role TEXT NOT NULL,"
        "  is_verified INTEGER NOT NULL,"
        "  updated_at_ms INTEGER NOT NULL"
        ");"
    );
    if (!q.exec(createSessions)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create cached_session table: ") + q.lastError().text();
        return false;
    }

    // 3. Local emergency records (Phase 2 legacy table)
    const QString createEmergency = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS local_emergency_records ("
        "  emergency_id TEXT PRIMARY KEY,"
        "  status TEXT NOT NULL,"
        "  started_at_ms INTEGER NOT NULL,"
        "  updated_at_ms INTEGER NOT NULL,"
        "  latitude REAL NOT NULL,"
        "  longitude REAL NOT NULL,"
        "  has_location INTEGER NOT NULL,"
        "  sync_status TEXT NOT NULL"
        ");"
    );
    if (!q.exec(createEmergency)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create local_emergency_records table: ") + q.lastError().text();
        return false;
    }

    // Record migration in schema_migrations
    q.prepare(QStringLiteral(
        "INSERT INTO schema_migrations (version, applied_at_ms, description) "
        "VALUES (:ver, :ts, :desc);"
    ));
    q.bindValue(QStringLiteral(":ver"), 1);
    q.bindValue(QStringLiteral(":ts"), QDateTime::currentMSecsSinceEpoch());
    q.bindValue(QStringLiteral(":desc"), QStringLiteral("Initial V1 Foundation: preferences, session cache, emergency scaffold"));

    if (!q.exec()) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to record migration V1: ") + q.lastError().text();
        return false;
    }

    if (!db.commit()) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to commit transaction for Schema V1: ") + db.lastError().text();
        return false;
    }

    return true;
}

bool LocalDatabase::executeSchemaV2() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Failed to begin transaction for Schema V2.");
        return false;
    }

    QSqlQuery q(db);

    // 1. Production Emergency Records table with full typed lifecycle
    const QString createEmergencyTable = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS emergency_records ("
        "  emergency_id TEXT PRIMARY KEY,"
        "  user_id TEXT NOT NULL,"
        "  status TEXT NOT NULL,"
        "  started_at_utc_ms INTEGER NOT NULL,"
        "  completed_at_utc_ms INTEGER,"
        "  latitude REAL,"
        "  longitude REAL,"
        "  location_accuracy_meters REAL,"
        "  location_timestamp_utc_ms INTEGER,"
        "  location_status TEXT NOT NULL,"
        "  note TEXT,"
        "  created_at_utc_ms INTEGER NOT NULL,"
        "  updated_at_utc_ms INTEGER NOT NULL,"
        "  sync_status TEXT NOT NULL,"
        "  schema_version INTEGER NOT NULL"
        ");"
    );
    if (!q.exec(createEmergencyTable)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create emergency_records table: ") + q.lastError().text();
        return false;
    }

    // 2. Partial unique index enforcing AT MOST ONE ACTIVE emergency per user
    const QString createActiveIndex = QStringLiteral(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_one_active_emergency_per_user "
        "ON emergency_records (user_id) WHERE status = 'ACTIVE';"
    );
    if (!q.exec(createActiveIndex)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create idx_one_active_emergency_per_user: ") + q.lastError().text();
        return false;
    }

    // 3. Reverse chronological history index
    const QString createHistoryIndex = QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_emergency_user_history "
        "ON emergency_records (user_id, started_at_utc_ms DESC);"
    );
    if (!q.exec(createHistoryIndex)) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to create idx_emergency_user_history: ") + q.lastError().text();
        return false;
    }

    // Record migration in schema_migrations
    q.prepare(QStringLiteral(
        "INSERT INTO schema_migrations (version, applied_at_ms, description) "
        "VALUES (:ver, :ts, :desc);"
    ));
    q.bindValue(QStringLiteral(":ver"), 2);
    q.bindValue(QStringLiteral(":ts"), QDateTime::currentMSecsSinceEpoch());
    q.bindValue(QStringLiteral(":desc"), QStringLiteral("Schema V2: Multi-user emergency lifecycle with partial unique active index"));

    if (!q.exec()) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to record migration V2: ") + q.lastError().text();
        return false;
    }

    if (!db.commit()) {
        db.rollback();
        m_lastError = QStringLiteral("Failed to commit transaction for Schema V2: ") + db.lastError().text();
        return false;
    }

    qInfo() << "[LocalDatabase] Applied Schema Migration V2: Emergency lifecycle & partial unique index.";
    return true;
}

bool LocalDatabase::setPreference(const QString &key, const QString &value) {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO app_preferences (pref_key, pref_value, updated_at_ms) "
        "VALUES (:key, :val, :ts) "
        "ON CONFLICT(pref_key) DO UPDATE SET pref_value = excluded.pref_value, updated_at_ms = excluded.updated_at_ms;"
    ));
    q.bindValue(QStringLiteral(":key"), key);
    q.bindValue(QStringLiteral(":val"), value);
    q.bindValue(QStringLiteral(":ts"), QDateTime::currentMSecsSinceEpoch());

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

QString LocalDatabase::getPreference(const QString &key, const QString &defaultValue) const {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized) return defaultValue;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT pref_value FROM app_preferences WHERE pref_key = :key;"));
    q.bindValue(QStringLiteral(":key"), key);

    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return defaultValue;
}

bool LocalDatabase::saveSession(const georescux::domain::UserProfile &profile) {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized || !profile.isValid()) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO cached_session (session_id, uid, display_name, email, phone, role, is_verified, updated_at_ms) "
        "VALUES ('active_session', :uid, :name, :email, :phone, :role, :ver, :ts);"
    ));
    q.bindValue(QStringLiteral(":uid"), profile.uid);
    q.bindValue(QStringLiteral(":name"), profile.displayName);
    q.bindValue(QStringLiteral(":email"), profile.email);
    q.bindValue(QStringLiteral(":phone"), profile.phone);
    q.bindValue(QStringLiteral(":role"), (profile.role == georescux::domain::UserRole::AuthorizedAdmin)
                                             ? QStringLiteral("ADMIN") : QStringLiteral("USER"));
    q.bindValue(QStringLiteral(":ver"), profile.isVerified ? 1 : 0);
    q.bindValue(QStringLiteral(":ts"), QDateTime::currentMSecsSinceEpoch());

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

std::optional<georescux::domain::UserProfile> LocalDatabase::loadSession() const {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized) return std::nullopt;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT uid, display_name, email, phone, role, is_verified, updated_at_ms FROM cached_session WHERE session_id = 'active_session';"));

    if (q.exec() && q.next()) {
        georescux::domain::UserProfile profile;
        profile.uid = q.value(0).toString();
        profile.displayName = q.value(1).toString();
        profile.email = q.value(2).toString();
        profile.phone = q.value(3).toString();
        const QString roleStr = q.value(4).toString();
        profile.role = (roleStr == QStringLiteral("ADMIN"))
                           ? georescux::domain::UserRole::AuthorizedAdmin
                           : georescux::domain::UserRole::CitizenUser;
        profile.isVerified = (q.value(5).toInt() != 0);
        profile.lastLoginMs = q.value(6).toLongLong();
        return profile;
    }
    return std::nullopt;
}

bool LocalDatabase::clearSession() {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    return q.exec(QStringLiteral("DELETE FROM cached_session WHERE session_id = 'active_session';"));
}

bool LocalDatabase::recordEmergencyScaffold(
    const QString &emergencyId,
    const QString &status,
    qint64 startedAtMs,
    double lat,
    double lon
) {
    QMutexLocker locker(&m_mutex);
    if (!checkThreadAffinity() || !m_isInitialized) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO local_emergency_records "
        "(emergency_id, status, started_at_ms, updated_at_ms, latitude, longitude, has_location, sync_status) "
        "VALUES (:id, :st, :start, :up, :lat, :lon, :hasLoc, 'PENDING');"
    ));
    q.bindValue(QStringLiteral(":id"), emergencyId);
    q.bindValue(QStringLiteral(":st"), status);
    q.bindValue(QStringLiteral(":start"), startedAtMs);
    q.bindValue(QStringLiteral(":up"), QDateTime::currentMSecsSinceEpoch());
    q.bindValue(QStringLiteral(":lat"), lat);
    q.bindValue(QStringLiteral(":lon"), lon);
    q.bindValue(QStringLiteral(":hasLoc"), (lat != 0.0 || lon != 0.0) ? 1 : 0);

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

void LocalDatabase::close() {
    QMutexLocker locker(&m_mutex);
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) {
                db.close();
            }
        } // Ensure db goes out of scope before removeDatabase
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_isInitialized = false;
    m_creatorThread = nullptr;
}

} // namespace georescux::storage
