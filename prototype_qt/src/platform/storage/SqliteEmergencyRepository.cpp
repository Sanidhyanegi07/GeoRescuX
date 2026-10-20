#include "SqliteEmergencyRepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

namespace georescux::storage {

using namespace georescux::domain;

SqliteEmergencyRepository::SqliteEmergencyRepository(std::shared_ptr<LocalDatabase> database)
    : m_database(std::move(database))
{
}

EmergencyRecord SqliteEmergencyRepository::recordFromQuery(QSqlQuery &q) const {
    EmergencyRecord rec;
    rec.emergencyId = q.value(0).toString();
    rec.userId = q.value(1).toString();
    rec.status = emergencyStatusFromString(q.value(2).toString());
    rec.startedAtUtcMs = q.value(3).toLongLong();

    if (!q.isNull(4)) {
        rec.completedAtUtcMs = q.value(4).toLongLong();
    }

    if (!q.isNull(5) && !q.isNull(6)) {
        EmergencyLocation loc;
        loc.latitude = q.value(5).toDouble();
        loc.longitude = q.value(6).toDouble();
        loc.accuracyMeters = q.value(7).toDouble();
        loc.timestampUtcMs = q.value(8).toLongLong();
        rec.location = loc;
    }

    rec.locationStatus = locationStatusFromString(q.value(9).toString());
    rec.note = q.value(10).toString();
    rec.createdAtUtcMs = q.value(11).toLongLong();
    rec.updatedAtUtcMs = q.value(12).toLongLong();
    rec.syncStatus = syncStatusFromString(q.value(13).toString());
    rec.schemaVersion = q.value(14).toInt();

    return rec;
}

EmergencyResult<std::optional<EmergencyRecord>>
SqliteEmergencyRepository::getActiveEmergency(const QString &userId) {
    if (userId.trimmed().isEmpty()) {
        return EmergencyResult<std::optional<EmergencyRecord>>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<std::optional<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not initialized or accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<std::optional<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records "
        "WHERE user_id = :uid AND status = 'ACTIVE' LIMIT 1;"
    ));
    q.bindValue(QStringLiteral(":uid"), userId.trimmed());

    if (!q.exec()) {
        return EmergencyResult<std::optional<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to query active emergency: ") + q.lastError().text()
        );
    }

    if (q.next()) {
        return EmergencyResult<std::optional<EmergencyRecord>>::ok(recordFromQuery(q));
    }

    return EmergencyResult<std::optional<EmergencyRecord>>::ok(std::nullopt);
}

EmergencyResult<EmergencyRecord>
SqliteEmergencyRepository::activateEmergency(const QString &userId, const QString &note) {
    const QString trimmedUid = userId.trimmed();
    if (trimmedUid.isEmpty()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not initialized or accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    if (!db.transaction()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to begin transaction for emergency activation.")
        );
    }

    // 1. Invariant check: Does this user already have an ACTIVE emergency?
    QSqlQuery checkQuery(db);
    checkQuery.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records "
        "WHERE user_id = :uid AND status = 'ACTIVE' LIMIT 1;"
    ));
    checkQuery.bindValue(QStringLiteral(":uid"), trimmedUid);

    if (checkQuery.exec() && checkQuery.next()) {
        EmergencyRecord existing = recordFromQuery(checkQuery);
        db.rollback();
        auto res = EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::AlreadyActive,
            QStringLiteral("An emergency is already active for this user: ") + existing.emergencyId
        );
        res.data = existing;
        return res;
    }

    // 2. Prepare new emergency record
    const qint64 nowUtcMs = QDateTime::currentMSecsSinceEpoch();
    const QString emergencyId = QStringLiteral("emer_") +
                                QString::number(nowUtcMs) +
                                QStringLiteral("_") +
                                QUuid::createUuid().toString(QUuid::Id128).left(8);

    // Enforce 120 char limit on note
    QString sanitizedNote = note.trimmed();
    if (sanitizedNote.length() > 120) {
        sanitizedNote = sanitizedNote.left(120);
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO emergency_records ("
        "  emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "  latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "  location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version"
        ") VALUES ("
        "  :id, :uid, 'ACTIVE', :started, NULL, "
        "  NULL, NULL, NULL, NULL, "
        "  'UNKNOWN', :note, :created, :updated, 'PENDING', 2"
        ");"
    ));
    insertQuery.bindValue(QStringLiteral(":id"), emergencyId);
    insertQuery.bindValue(QStringLiteral(":uid"), trimmedUid);
    insertQuery.bindValue(QStringLiteral(":started"), nowUtcMs);
    insertQuery.bindValue(QStringLiteral(":note"), sanitizedNote.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : sanitizedNote);
    insertQuery.bindValue(QStringLiteral(":created"), nowUtcMs);
    insertQuery.bindValue(QStringLiteral(":updated"), nowUtcMs);

    if (!insertQuery.exec()) {
        const QString errText = insertQuery.lastError().text();
        db.rollback();

        // Check if SQLite partial unique index rejected a concurrent activation
        if (errText.contains(QStringLiteral("UNIQUE constraint failed"), Qt::CaseInsensitive) ||
            errText.contains(QStringLiteral("idx_one_active_emergency_per_user"), Qt::CaseInsensitive)) {
            return EmergencyResult<EmergencyRecord>::fail(
                EmergencyErrorCode::AlreadyActive,
                QStringLiteral("Database partial unique constraint rejected duplicate active emergency.")
            );
        }

        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to insert active emergency: ") + errText
        );
    }

    if (!db.commit()) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to commit emergency activation transaction.")
        );
    }

    EmergencyRecord newRecord;
    newRecord.emergencyId = emergencyId;
    newRecord.userId = trimmedUid;
    newRecord.status = EmergencyStatus::Active;
    newRecord.startedAtUtcMs = nowUtcMs;
    newRecord.completedAtUtcMs = std::nullopt;
    newRecord.location = std::nullopt;
    newRecord.locationStatus = EmergencyLocationStatus::Unknown;
    newRecord.note = sanitizedNote;
    newRecord.createdAtUtcMs = nowUtcMs;
    newRecord.updatedAtUtcMs = nowUtcMs;
    newRecord.syncStatus = EmergencySyncStatus::Pending;
    newRecord.schemaVersion = 2;

    qInfo().noquote() << "[SqliteEmergencyRepository] Activated emergency:" << emergencyId
                      << "for user:" << trimmedUid;
    return EmergencyResult<EmergencyRecord>::ok(newRecord);
}

EmergencyResult<EmergencyRecord>
SqliteEmergencyRepository::updateLocation(
    const QString &userId,
    const QString &emergencyId,
    const EmergencyLocation &location,
    EmergencyLocationStatus status
) {
    const QString trimmedUid = userId.trimmed();
    const QString trimmedEid = emergencyId.trimmed();

    if (trimmedUid.isEmpty() || trimmedEid.isEmpty()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID and Emergency ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());

    // Check ownership and state to distinguish NotFound from InvalidState without leaking foreign records
    QSqlQuery checkQ(db);
    checkQ.prepare(QStringLiteral(
        "SELECT status, user_id FROM emergency_records WHERE emergency_id = :id;"
    ));
    checkQ.bindValue(QStringLiteral(":id"), trimmedEid);

    if (!checkQ.exec() || !checkQ.next()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency not found: ") + trimmedEid
        );
    }

    const QString recordUid = checkQ.value(1).toString();
    if (recordUid != trimmedUid) {
        // Do not reveal another user's emergency details
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency record not found for user: ") + trimmedUid
        );
    }

    const QString recordStatus = checkQ.value(0).toString();
    if (recordStatus != QStringLiteral("ACTIVE")) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidState,
            QStringLiteral("Emergency is not active: ") + trimmedEid
        );
    }

    const qint64 nowUtcMs = QDateTime::currentMSecsSinceEpoch();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE emergency_records SET "
        "  latitude = :lat, "
        "  longitude = :lon, "
        "  location_accuracy_meters = :acc, "
        "  location_timestamp_utc_ms = :lts, "
        "  location_status = :lst, "
        "  updated_at_utc_ms = :ts "
        "WHERE emergency_id = :id AND user_id = :uid AND status = 'ACTIVE';"
    ));
    if (location.isValid()) {
        q.bindValue(QStringLiteral(":lat"), location.latitude);
        q.bindValue(QStringLiteral(":lon"), location.longitude);
        q.bindValue(QStringLiteral(":acc"), location.accuracyMeters);
        q.bindValue(QStringLiteral(":lts"), location.timestampUtcMs > 0 ? location.timestampUtcMs : nowUtcMs);
    } else {
        q.bindValue(QStringLiteral(":lat"), QVariant(QMetaType(QMetaType::Double)));
        q.bindValue(QStringLiteral(":lon"), QVariant(QMetaType(QMetaType::Double)));
        q.bindValue(QStringLiteral(":acc"), QVariant(QMetaType(QMetaType::Double)));
        q.bindValue(QStringLiteral(":lts"), QVariant(QMetaType(QMetaType::LongLong)));
    }
    q.bindValue(QStringLiteral(":lst"), locationStatusToString(status));
    q.bindValue(QStringLiteral(":ts"), nowUtcMs);
    q.bindValue(QStringLiteral(":id"), trimmedEid);
    q.bindValue(QStringLiteral(":uid"), trimmedUid);

    if (!q.exec() || q.numRowsAffected() == 0) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to update location: ") + q.lastError().text()
        );
    }

    // Retrieve updated record
    QSqlQuery fetchQ(db);
    fetchQ.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records WHERE emergency_id = :id AND user_id = :uid;"
    ));
    fetchQ.bindValue(QStringLiteral(":id"), trimmedEid);
    fetchQ.bindValue(QStringLiteral(":uid"), trimmedUid);
    if (fetchQ.exec() && fetchQ.next()) {
        return EmergencyResult<EmergencyRecord>::ok(recordFromQuery(fetchQ));
    }

    return EmergencyResult<EmergencyRecord>::fail(EmergencyErrorCode::NotFound, QStringLiteral("Record missing after update."));
}

EmergencyResult<EmergencyRecord>
SqliteEmergencyRepository::updateNote(const QString &userId, const QString &emergencyId, const QString &note) {
    const QString trimmedUid = userId.trimmed();
    const QString trimmedEid = emergencyId.trimmed();

    if (trimmedUid.isEmpty() || trimmedEid.isEmpty()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID and Emergency ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());

    // Check ownership and state to distinguish NotFound from InvalidState without leaking foreign records
    QSqlQuery checkQ(db);
    checkQ.prepare(QStringLiteral(
        "SELECT status, user_id FROM emergency_records WHERE emergency_id = :id;"
    ));
    checkQ.bindValue(QStringLiteral(":id"), trimmedEid);

    if (!checkQ.exec() || !checkQ.next()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency not found: ") + trimmedEid
        );
    }

    const QString recordUid = checkQ.value(1).toString();
    if (recordUid != trimmedUid) {
        // Do not reveal another user's emergency details
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency record not found for user: ") + trimmedUid
        );
    }

    const QString recordStatus = checkQ.value(0).toString();
    if (recordStatus != QStringLiteral("ACTIVE")) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidState,
            QStringLiteral("Emergency is not active: ") + trimmedEid
        );
    }

    QString sanitizedNote = note.trimmed();
    if (sanitizedNote.length() > 120) {
        sanitizedNote = sanitizedNote.left(120);
    }

    const qint64 nowUtcMs = QDateTime::currentMSecsSinceEpoch();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE emergency_records SET note = :note, updated_at_utc_ms = :ts "
        "WHERE emergency_id = :id AND user_id = :uid AND status = 'ACTIVE';"
    ));
    q.bindValue(QStringLiteral(":note"), sanitizedNote.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : sanitizedNote);
    q.bindValue(QStringLiteral(":ts"), nowUtcMs);
    q.bindValue(QStringLiteral(":id"), trimmedEid);
    q.bindValue(QStringLiteral(":uid"), trimmedUid);

    if (!q.exec() || q.numRowsAffected() == 0) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to update note: ") + q.lastError().text()
        );
    }

    // Retrieve updated record
    QSqlQuery fetchQ(db);
    fetchQ.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records WHERE emergency_id = :id AND user_id = :uid;"
    ));
    fetchQ.bindValue(QStringLiteral(":id"), trimmedEid);
    fetchQ.bindValue(QStringLiteral(":uid"), trimmedUid);
    if (fetchQ.exec() && fetchQ.next()) {
        return EmergencyResult<EmergencyRecord>::ok(recordFromQuery(fetchQ));
    }

    return EmergencyResult<EmergencyRecord>::fail(EmergencyErrorCode::NotFound, QStringLiteral("Record missing after update."));
}

EmergencyResult<EmergencyRecord>
SqliteEmergencyRepository::completeEmergency(const QString &userId, const QString &emergencyId) {
    const QString trimmedUid = userId.trimmed();
    const QString trimmedEid = emergencyId.trimmed();

    if (trimmedUid.isEmpty() || trimmedEid.isEmpty()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID and Emergency ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    if (!db.transaction()) {
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to begin transaction for completion.")
        );
    }

    // Check existing state to distinguish NotFound from InvalidState
    QSqlQuery checkQ(db);
    checkQ.prepare(QStringLiteral(
        "SELECT status, user_id FROM emergency_records WHERE emergency_id = :id;"
    ));
    checkQ.bindValue(QStringLiteral(":id"), trimmedEid);

    if (!checkQ.exec() || !checkQ.next()) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency ID not found: ") + trimmedEid
        );
    }

    const QString recordUid = checkQ.value(1).toString();
    if (recordUid != trimmedUid) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::NotFound,
            QStringLiteral("Emergency record does not belong to user: ") + trimmedUid
        );
    }

    const QString recordStatus = checkQ.value(0).toString();
    if (recordStatus == QStringLiteral("COMPLETED")) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::InvalidState,
            QStringLiteral("Emergency is already completed: ") + trimmedEid
        );
    }

    const qint64 nowUtcMs = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery updateQ(db);
    updateQ.prepare(QStringLiteral(
        "UPDATE emergency_records SET "
        "  status = 'COMPLETED', "
        "  completed_at_utc_ms = :comp, "
        "  updated_at_utc_ms = :comp "
        "WHERE emergency_id = :id AND user_id = :uid AND status = 'ACTIVE';"
    ));
    updateQ.bindValue(QStringLiteral(":comp"), nowUtcMs);
    updateQ.bindValue(QStringLiteral(":id"), trimmedEid);
    updateQ.bindValue(QStringLiteral(":uid"), trimmedUid);

    if (!updateQ.exec() || updateQ.numRowsAffected() == 0) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to complete emergency in database.")
        );
    }

    if (!db.commit()) {
        db.rollback();
        return EmergencyResult<EmergencyRecord>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to commit completion transaction.")
        );
    }

    // Retrieve completed record
    QSqlQuery fetchQ(db);
    fetchQ.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records WHERE emergency_id = :id;"
    ));
    fetchQ.bindValue(QStringLiteral(":id"), trimmedEid);
    if (fetchQ.exec() && fetchQ.next()) {
        EmergencyRecord completed = recordFromQuery(fetchQ);
        qInfo().noquote() << "[SqliteEmergencyRepository] Completed emergency:" << trimmedEid
                          << "for user:" << trimmedUid;
        return EmergencyResult<EmergencyRecord>::ok(completed);
    }

    return EmergencyResult<EmergencyRecord>::fail(EmergencyErrorCode::NotFound, QStringLiteral("Record missing after completion."));
}

EmergencyResult<std::vector<EmergencyRecord>>
SqliteEmergencyRepository::getEmergencyHistory(const QString &userId, int limit) {
    const QString trimmedUid = userId.trimmed();
    if (trimmedUid.isEmpty()) {
        return EmergencyResult<std::vector<EmergencyRecord>>::fail(
            EmergencyErrorCode::InvalidInput,
            QStringLiteral("User ID must not be empty.")
        );
    }

    if (!m_database || !m_database->isOpen()) {
        return EmergencyResult<std::vector<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageUnavailable,
            QStringLiteral("Database is not accessible.")
        );
    }

    QMutexLocker locker(&m_database->mutex());
    if (!m_database->checkThreadAffinity()) {
        return EmergencyResult<std::vector<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageUnavailable,
            m_database->lastError()
        );
    }

    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT emergency_id, user_id, status, started_at_utc_ms, completed_at_utc_ms, "
        "       latitude, longitude, location_accuracy_meters, location_timestamp_utc_ms, "
        "       location_status, note, created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version "
        "FROM emergency_records "
        "WHERE user_id = :uid "
        "ORDER BY started_at_utc_ms DESC LIMIT :lim;"
    ));
    q.bindValue(QStringLiteral(":uid"), trimmedUid);
    q.bindValue(QStringLiteral(":lim"), std::max(1, limit));

    if (!q.exec()) {
        return EmergencyResult<std::vector<EmergencyRecord>>::fail(
            EmergencyErrorCode::StorageFailure,
            QStringLiteral("Failed to query emergency history: ") + q.lastError().text()
        );
    }

    std::vector<EmergencyRecord> history;
    while (q.next()) {
        history.push_back(recordFromQuery(q));
    }

    return EmergencyResult<std::vector<EmergencyRecord>>::ok(history);
}

} // namespace georescux::storage
