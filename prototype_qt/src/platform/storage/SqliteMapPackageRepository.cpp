#include "SqliteMapPackageRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

namespace georescux::storage {

using namespace georescux::domain;

SqliteMapPackageRepository::SqliteMapPackageRepository(std::shared_ptr<LocalDatabase> database)
    : m_database(std::move(database)) {
}

bool SqliteMapPackageRepository::ensureTableCreated() {
    if (!m_database || !m_database->isOpen()) return false;
    if (m_tableEnsured) return true;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    const QString createSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS installed_map_packages ("
        "  region_id TEXT PRIMARY KEY,"
        "  display_name TEXT NOT NULL,"
        "  package_version INTEGER NOT NULL,"
        "  supported_format TEXT NOT NULL,"
        "  min_lat REAL NOT NULL,"
        "  max_lat REAL NOT NULL,"
        "  min_lon REAL NOT NULL,"
        "  max_lon REAL NOT NULL,"
        "  map_file_path TEXT,"
        "  graph_file_path TEXT,"
        "  places_file_path TEXT,"
        "  boundary_file_path TEXT,"
        "  file_size_bytes INTEGER NOT NULL,"
        "  sha256_checksum TEXT,"
        "  routing_data_version INTEGER NOT NULL,"
        "  installed_at_utc_ms INTEGER NOT NULL,"
        "  package_status TEXT NOT NULL,"
        "  has_rendering_data INTEGER NOT NULL,"
        "  has_routing_data INTEGER NOT NULL,"
        "  is_active INTEGER NOT NULL DEFAULT 0"
        ");"
    );

    if (!q.exec(createSql)) {
        qWarning() << "[SqliteMapPackageRepository] Failed to create installed_map_packages table:" << q.lastError().text();
        return false;
    }

    m_tableEnsured = true;
    return true;
}

static MapPackage rowToPackage(const QSqlQuery &q) {
    MapPackage pkg;
    pkg.regionId = q.value(0).toString();
    pkg.displayName = q.value(1).toString();
    pkg.packageVersion = q.value(2).toInt();
    pkg.supportedFormat = q.value(3).toString();
    pkg.coverage.minLatitude = q.value(4).toDouble();
    pkg.coverage.maxLatitude = q.value(5).toDouble();
    pkg.coverage.minLongitude = q.value(6).toDouble();
    pkg.coverage.maxLongitude = q.value(7).toDouble();
    pkg.mapFilePath = q.value(8).toString();
    pkg.graphFilePath = q.value(9).toString();
    pkg.placesFilePath = q.value(10).toString();
    pkg.boundaryFilePath = q.value(11).toString();
    pkg.fileSizeBytes = q.value(12).toLongLong();
    pkg.sha256Checksum = q.value(13).toString();
    pkg.routingDataVersion = q.value(14).toInt();
    pkg.installedAtUtcMs = q.value(15).toLongLong();
    pkg.status = stringToPackageStatus(q.value(16).toString());
    pkg.hasRenderingData = q.value(17).toInt() != 0;
    pkg.hasRoutingData = q.value(18).toInt() != 0;
    return pkg;
}

bool SqliteMapPackageRepository::savePackage(const MapPackage &pkg) {
    if (!ensureTableCreated()) return false;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    q.prepare(QStringLiteral(
        "INSERT INTO installed_map_packages ("
        "  region_id, display_name, package_version, supported_format,"
        "  min_lat, max_lat, min_lon, max_lon,"
        "  map_file_path, graph_file_path, places_file_path, boundary_file_path,"
        "  file_size_bytes, sha256_checksum, routing_data_version,"
        "  installed_at_utc_ms, package_status, has_rendering_data, has_routing_data, is_active"
        ") VALUES ("
        "  :rid, :name, :ver, :fmt, :min_lat, :max_lat, :min_lon, :max_lon,"
        "  :map, :graph, :places, :bound, :sz, :sha, :rver, :ts, :status, :has_rend, :has_rout, :active"
        ") ON CONFLICT(region_id) DO UPDATE SET "
        "  display_name = excluded.display_name,"
        "  package_version = excluded.package_version,"
        "  supported_format = excluded.supported_format,"
        "  min_lat = excluded.min_lat,"
        "  max_lat = excluded.max_lat,"
        "  min_lon = excluded.min_lon,"
        "  max_lon = excluded.max_lon,"
        "  map_file_path = excluded.map_file_path,"
        "  graph_file_path = excluded.graph_file_path,"
        "  places_file_path = excluded.places_file_path,"
        "  boundary_file_path = excluded.boundary_file_path,"
        "  file_size_bytes = excluded.file_size_bytes,"
        "  sha256_checksum = excluded.sha256_checksum,"
        "  routing_data_version = excluded.routing_data_version,"
        "  installed_at_utc_ms = excluded.installed_at_utc_ms,"
        "  package_status = excluded.package_status,"
        "  has_rendering_data = excluded.has_rendering_data,"
        "  has_routing_data = excluded.has_routing_data,"
        "  is_active = excluded.is_active;"
    ));

    q.bindValue(QStringLiteral(":rid"), pkg.regionId);
    q.bindValue(QStringLiteral(":name"), pkg.displayName);
    q.bindValue(QStringLiteral(":ver"), pkg.packageVersion);
    q.bindValue(QStringLiteral(":fmt"), pkg.supportedFormat);
    q.bindValue(QStringLiteral(":min_lat"), pkg.coverage.minLatitude);
    q.bindValue(QStringLiteral(":max_lat"), pkg.coverage.maxLatitude);
    q.bindValue(QStringLiteral(":min_lon"), pkg.coverage.minLongitude);
    q.bindValue(QStringLiteral(":max_lon"), pkg.coverage.maxLongitude);
    q.bindValue(QStringLiteral(":map"), pkg.mapFilePath);
    q.bindValue(QStringLiteral(":graph"), pkg.graphFilePath);
    q.bindValue(QStringLiteral(":places"), pkg.placesFilePath);
    q.bindValue(QStringLiteral(":bound"), pkg.boundaryFilePath);
    q.bindValue(QStringLiteral(":sz"), pkg.fileSizeBytes);
    q.bindValue(QStringLiteral(":sha"), pkg.sha256Checksum);
    q.bindValue(QStringLiteral(":rver"), pkg.routingDataVersion);
    q.bindValue(QStringLiteral(":ts"), pkg.installedAtUtcMs);
    q.bindValue(QStringLiteral(":status"), packageStatusToString(pkg.status));
    q.bindValue(QStringLiteral(":has_rend"), pkg.hasRenderingData ? 1 : 0);
    q.bindValue(QStringLiteral(":has_rout"), pkg.hasRoutingData ? 1 : 0);
    q.bindValue(QStringLiteral(":active"), (pkg.status == PackageStatus::Active) ? 1 : 0);

    if (!q.exec()) {
        qWarning() << "[SqliteMapPackageRepository] Failed to save package:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<MapPackage> SqliteMapPackageRepository::getPackage(const QString &regionId) {
    if (!ensureTableCreated()) return std::nullopt;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    q.prepare(QStringLiteral(
        "SELECT region_id, display_name, package_version, supported_format,"
        "       min_lat, max_lat, min_lon, max_lon,"
        "       map_file_path, graph_file_path, places_file_path, boundary_file_path,"
        "       file_size_bytes, sha256_checksum, routing_data_version,"
        "       installed_at_utc_ms, package_status, has_rendering_data, has_routing_data "
        "FROM installed_map_packages WHERE region_id = :rid;"
    ));
    q.bindValue(QStringLiteral(":rid"), regionId.trimmed());

    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }
    return rowToPackage(q);
}

std::vector<MapPackage> SqliteMapPackageRepository::listPackages() {
    std::vector<MapPackage> result;
    if (!ensureTableCreated()) return result;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    const QString selectSql = QStringLiteral(
        "SELECT region_id, display_name, package_version, supported_format,"
        "       min_lat, max_lat, min_lon, max_lon,"
        "       map_file_path, graph_file_path, places_file_path, boundary_file_path,"
        "       file_size_bytes, sha256_checksum, routing_data_version,"
        "       installed_at_utc_ms, package_status, has_rendering_data, has_routing_data "
        "FROM installed_map_packages ORDER BY display_name ASC;"
    );

    if (!q.exec(selectSql)) return result;

    while (q.next()) {
        result.push_back(rowToPackage(q));
    }
    return result;
}

bool SqliteMapPackageRepository::deletePackage(const QString &regionId) {
    if (!ensureTableCreated()) return false;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    q.prepare(QStringLiteral("DELETE FROM installed_map_packages WHERE region_id = :rid;"));
    q.bindValue(QStringLiteral(":rid"), regionId.trimmed());
    return q.exec();
}

std::optional<MapPackage> SqliteMapPackageRepository::getActivePackage() {
    if (!ensureTableCreated()) return std::nullopt;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    QSqlQuery q(db);

    const QString selectSql = QStringLiteral(
        "SELECT region_id, display_name, package_version, supported_format,"
        "       min_lat, max_lat, min_lon, max_lon,"
        "       map_file_path, graph_file_path, places_file_path, boundary_file_path,"
        "       file_size_bytes, sha256_checksum, routing_data_version,"
        "       installed_at_utc_ms, package_status, has_rendering_data, has_routing_data "
        "FROM installed_map_packages WHERE is_active = 1 LIMIT 1;"
    );

    if (!q.exec(selectSql) || !q.next()) {
        return std::nullopt;
    }
    return rowToPackage(q);
}

bool SqliteMapPackageRepository::setActivePackage(const QString &regionId) {
    if (!ensureTableCreated()) return false;

    QMutexLocker locker(&m_database->mutex());
    QSqlDatabase db = QSqlDatabase::database(m_database->connectionName());
    if (!db.transaction()) return false;

    QSqlQuery qClear(db);
    if (!qClear.exec(QStringLiteral("UPDATE installed_map_packages SET is_active = 0, package_status = 'INSTALLED' WHERE is_active = 1;"))) {
        db.rollback();
        return false;
    }

    QSqlQuery qSet(db);
    qSet.prepare(QStringLiteral("UPDATE installed_map_packages SET is_active = 1, package_status = 'ACTIVE' WHERE region_id = :rid;"));
    qSet.bindValue(QStringLiteral(":rid"), regionId.trimmed());

    if (!qSet.exec() || qSet.numRowsAffected() == 0) {
        db.rollback();
        return false;
    }

    return db.commit();
}

} // namespace georescux::storage
