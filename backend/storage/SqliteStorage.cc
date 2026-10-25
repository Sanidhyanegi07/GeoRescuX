#include "SqliteStorage.h"
#include <chrono>
#include <iostream>

namespace georescux::backend::storage {

namespace {

long long currentEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string safeColumnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

inline std::optional<std::string> safeColumnOptionalText(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) return std::nullopt;
    const unsigned char* text = sqlite3_column_text(stmt, col);
    if (!text) return std::nullopt;
    return std::string(reinterpret_cast<const char*>(text));
}

} // namespace

SqliteStorage::SqliteStorage(const std::string& dbPath) : dbPath_(dbPath) {}

SqliteStorage::~SqliteStorage() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SqliteStorage::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    return createTables();
}

bool SqliteStorage::createTables() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS sos_events ("
        "  event_id TEXT PRIMARY KEY,"
        "  user_id TEXT,"
        "  started_at_ms INTEGER,"
        "  stopped_at_ms INTEGER,"
        "  is_active INTEGER,"
        "  latitude REAL,"
        "  longitude REAL,"
        "  location_status TEXT,"
        "  note TEXT,"
        "  source TEXT,"
        "  received_at_ms INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS mesh_packets ("
        "  packet_id TEXT PRIMARY KEY,"
        "  origin_device_id TEXT,"
        "  origin_emergency_id TEXT,"
        "  timestamp_ms INTEGER,"
        "  ttl INTEGER,"
        "  type TEXT,"
        "  latitude REAL,"
        "  longitude REAL,"
        "  status TEXT,"
        "  note_text TEXT,"
        "  ingested_at_ms INTEGER"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to create tables: " << (errMsg ? errMsg : "unknown") << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool SqliteStorage::saveSosEvent(const models::SosEventDto& event, bool& wasDuplicate) {
    std::lock_guard<std::mutex> lock(mutex_);
    wasDuplicate = false;

    // Check if event already exists
    const char* checkSql = "SELECT event_id FROM sos_events WHERE event_id = ?;";
    sqlite3_stmt* checkStmt = nullptr;
    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, event.eventId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            wasDuplicate = true;
        }
        sqlite3_finalize(checkStmt);
    }

    const char* sql =
        "INSERT OR REPLACE INTO sos_events "
        "(event_id, user_id, started_at_ms, stopped_at_ms, is_active, latitude, longitude, location_status, note, source, received_at_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    long long nowMs = currentEpochMs();
    sqlite3_bind_text(stmt, 1, event.eventId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, event.userId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, event.startedAtMs);
    sqlite3_bind_int64(stmt, 4, event.stoppedAtMs);
    sqlite3_bind_int(stmt, 5, event.isActive ? 1 : 0);

    if (event.latitude) sqlite3_bind_double(stmt, 6, *event.latitude);
    else sqlite3_bind_null(stmt, 6);

    if (event.longitude) sqlite3_bind_double(stmt, 7, *event.longitude);
    else sqlite3_bind_null(stmt, 7);

    sqlite3_bind_text(stmt, 8, event.locationStatus.c_str(), -1, SQLITE_STATIC);

    if (event.note) sqlite3_bind_text(stmt, 9, event.note->c_str(), -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 9);

    sqlite3_bind_text(stmt, 10, event.source.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 11, nowMs);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::optional<models::SosEventDto> SqliteStorage::getSosEvent(const std::string& eventId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql =
        "SELECT event_id, user_id, started_at_ms, stopped_at_ms, is_active, latitude, longitude, location_status, note, source, received_at_ms "
        "FROM sos_events WHERE event_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, eventId.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    models::SosEventDto dto;
    dto.eventId = safeColumnText(stmt, 0);
    dto.userId = safeColumnText(stmt, 1);
    dto.startedAtMs = sqlite3_column_int64(stmt, 2);
    dto.stoppedAtMs = sqlite3_column_int64(stmt, 3);
    dto.isActive = (sqlite3_column_int(stmt, 4) != 0);

    if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) dto.latitude = sqlite3_column_double(stmt, 5);
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) dto.longitude = sqlite3_column_double(stmt, 6);

    dto.locationStatus = safeColumnText(stmt, 7);
    dto.note = safeColumnOptionalText(stmt, 8);
    dto.source = safeColumnText(stmt, 9);
    dto.receivedAtMs = sqlite3_column_int64(stmt, 10);

    sqlite3_finalize(stmt);
    return dto;
}

std::vector<models::SosEventDto> SqliteStorage::getAllSosEvents(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<models::SosEventDto> results;

    const char* sql =
        "SELECT event_id, user_id, started_at_ms, stopped_at_ms, is_active, latitude, longitude, location_status, note, source, received_at_ms "
        "FROM sos_events ORDER BY received_at_ms DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        models::SosEventDto dto;
        dto.eventId = safeColumnText(stmt, 0);
        dto.userId = safeColumnText(stmt, 1);
        dto.startedAtMs = sqlite3_column_int64(stmt, 2);
        dto.stoppedAtMs = sqlite3_column_int64(stmt, 3);
        dto.isActive = (sqlite3_column_int(stmt, 4) != 0);

        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) dto.latitude = sqlite3_column_double(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) dto.longitude = sqlite3_column_double(stmt, 6);

        dto.locationStatus = safeColumnText(stmt, 7);
        dto.note = safeColumnOptionalText(stmt, 8);
        dto.source = safeColumnText(stmt, 9);
        dto.receivedAtMs = sqlite3_column_int64(stmt, 10);
        results.push_back(std::move(dto));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool SqliteStorage::saveMeshPacket(const models::BleMeshPacketDto& packet, bool& wasDuplicate) {
    std::lock_guard<std::mutex> lock(mutex_);
    wasDuplicate = false;

    // Deduplication check
    const char* checkSql = "SELECT packet_id FROM mesh_packets WHERE packet_id = ?;";
    sqlite3_stmt* checkStmt = nullptr;
    if (sqlite3_prepare_v2(db_, checkSql, -1, &checkStmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(checkStmt, 1, packet.packetId.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(checkStmt) == SQLITE_ROW) {
            wasDuplicate = true;
        }
        sqlite3_finalize(checkStmt);
    }

    const char* sql =
        "INSERT OR REPLACE INTO mesh_packets "
        "(packet_id, origin_device_id, origin_emergency_id, timestamp_ms, ttl, type, latitude, longitude, status, note_text, ingested_at_ms) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    long long nowMs = currentEpochMs();
    sqlite3_bind_text(stmt, 1, packet.packetId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, packet.originDeviceId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, packet.originEmergencyId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, packet.timestampMs);
    sqlite3_bind_int(stmt, 5, packet.ttl);
    sqlite3_bind_text(stmt, 6, packet.type.c_str(), -1, SQLITE_STATIC);

    if (packet.latitude) sqlite3_bind_double(stmt, 7, *packet.latitude);
    else sqlite3_bind_null(stmt, 7);

    if (packet.longitude) sqlite3_bind_double(stmt, 8, *packet.longitude);
    else sqlite3_bind_null(stmt, 8);

    sqlite3_bind_text(stmt, 9, packet.status.c_str(), -1, SQLITE_STATIC);

    if (packet.noteText) sqlite3_bind_text(stmt, 10, packet.noteText->c_str(), -1, SQLITE_STATIC);
    else sqlite3_bind_null(stmt, 10);

    sqlite3_bind_int64(stmt, 11, nowMs);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::optional<models::BleMeshPacketDto> SqliteStorage::getMeshPacket(const std::string& packetId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql =
        "SELECT packet_id, origin_device_id, origin_emergency_id, timestamp_ms, ttl, type, latitude, longitude, status, note_text, ingested_at_ms "
        "FROM mesh_packets WHERE packet_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, packetId.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    models::BleMeshPacketDto dto;
    dto.packetId = safeColumnText(stmt, 0);
    dto.originDeviceId = safeColumnText(stmt, 1);
    dto.originEmergencyId = safeColumnText(stmt, 2);
    dto.timestampMs = sqlite3_column_int64(stmt, 3);
    dto.ttl = sqlite3_column_int(stmt, 4);
    dto.type = safeColumnText(stmt, 5);

    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) dto.latitude = sqlite3_column_double(stmt, 6);
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) dto.longitude = sqlite3_column_double(stmt, 7);

    dto.status = safeColumnText(stmt, 8);
    dto.noteText = safeColumnOptionalText(stmt, 9);
    dto.ingestedAtMs = sqlite3_column_int64(stmt, 10);

    sqlite3_finalize(stmt);
    return dto;
}

std::vector<models::BleMeshPacketDto> SqliteStorage::getAllMeshPackets(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<models::BleMeshPacketDto> results;

    const char* sql =
        "SELECT packet_id, origin_device_id, origin_emergency_id, timestamp_ms, ttl, type, latitude, longitude, status, note_text, ingested_at_ms "
        "FROM mesh_packets ORDER BY ingested_at_ms DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        models::BleMeshPacketDto dto;
        dto.packetId = safeColumnText(stmt, 0);
        dto.originDeviceId = safeColumnText(stmt, 1);
        dto.originEmergencyId = safeColumnText(stmt, 2);
        dto.timestampMs = sqlite3_column_int64(stmt, 3);
        dto.ttl = sqlite3_column_int(stmt, 4);
        dto.type = safeColumnText(stmt, 5);

        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) dto.latitude = sqlite3_column_double(stmt, 6);
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) dto.longitude = sqlite3_column_double(stmt, 7);

        dto.status = safeColumnText(stmt, 8);
        dto.noteText = safeColumnOptionalText(stmt, 9);
        dto.ingestedAtMs = sqlite3_column_int64(stmt, 10);
        results.push_back(std::move(dto));
    }
    sqlite3_finalize(stmt);
    return results;
}

} // namespace georescux::backend::storage
