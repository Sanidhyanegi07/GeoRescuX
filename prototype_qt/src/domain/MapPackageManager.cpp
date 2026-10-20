#include "MapPackageManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>

namespace georescux::domain {

MapPackageManager::MapPackageManager(
    std::shared_ptr<IMapPackageRepository> repository,
    QString baseInstallDir,
    QObject *parent
) : QObject(parent),
    m_repository(std::move(repository)),
    m_baseInstallDir(std::move(baseInstallDir)) {

    if (m_baseInstallDir.isEmpty()) {
        const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_baseInstallDir = QDir(appData).filePath(QStringLiteral("map_packages"));
    }
    QDir().mkpath(m_baseInstallDir);
}

QString MapPackageManager::activeRegionId() const {
    if (!m_repository) return QString();
    auto active = m_repository->getActivePackage();
    return active ? active->regionId : QString();
}

QString MapPackageManager::activeRegionDisplayName() const {
    if (!m_repository) return QString();
    auto active = m_repository->getActivePackage();
    return active ? active->displayName : QString();
}

bool MapPackageManager::hasActivePackage() const {
    if (!m_repository) return false;
    return m_repository->getActivePackage().has_value();
}

bool MapPackageManager::activeHasRenderingData() const {
    if (!m_repository) return false;
    auto active = m_repository->getActivePackage();
    return active ? active->hasRenderingData : false;
}

bool MapPackageManager::activeHasRoutingData() const {
    if (!m_repository) return false;
    auto active = m_repository->getActivePackage();
    return active ? active->hasRoutingData : false;
}

bool MapPackageManager::isPathTraversal(const QString &baseDir, const QString &targetPath) const {
    if (targetPath.contains(QStringLiteral("..")) ||
        targetPath.startsWith(QStringLiteral("/")) ||
        targetPath.startsWith(QStringLiteral("\\"))) {
        return true;
    }
    const QString cleanBase = QDir::cleanPath(baseDir);
    const QString cleanTarget = QDir::cleanPath(QDir(baseDir).filePath(targetPath));
    return !cleanTarget.startsWith(cleanBase);
}

bool MapPackageManager::validatePackageManifest(
    const QString &manifestPath,
    MapPackage &outPkg,
    QStringList &outErrors
) {
    outErrors.clear();
    QFile file(manifestPath);
    if (!file.exists()) {
        outErrors.append(QStringLiteral("Manifest file does not exist: %1").arg(manifestPath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        outErrors.append(QStringLiteral("Failed to open manifest file: %1").arg(manifestPath));
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        outErrors.append(QStringLiteral("Malformed JSON in manifest: %1").arg(parseErr.errorString()));
        return false;
    }

    const QJsonObject root = doc.object();
    outPkg.regionId = root.value(QStringLiteral("regionId")).toString().trimmed();
    outPkg.displayName = root.value(QStringLiteral("displayName")).toString().trimmed();
    outPkg.packageVersion = root.value(QStringLiteral("packageVersion")).toInt(1);
    outPkg.supportedFormat = root.value(QStringLiteral("renderingFormat")).toString(QStringLiteral("Mapsforge MAP"));
    outPkg.routingDataVersion = root.value(QStringLiteral("routingDataVersion")).toInt(1);

    if (outPkg.regionId.isEmpty()) {
        outErrors.append(QStringLiteral("Missing or empty 'regionId' in manifest."));
    }
    if (outPkg.displayName.isEmpty()) {
        outErrors.append(QStringLiteral("Missing or empty 'displayName' in manifest."));
    }
    if (outPkg.packageVersion <= 0) {
        outErrors.append(QStringLiteral("Invalid packageVersion: must be > 0."));
    }

    const QFileInfo manifestInfo(manifestPath);
    const QDir packageDir = manifestInfo.dir();

    // Check coverage
    if (root.contains(QStringLiteral("coverage")) && root.value(QStringLiteral("coverage")).isObject()) {
        const QJsonObject cov = root.value(QStringLiteral("coverage")).toObject();
        outPkg.coverage.minLatitude = cov.value(QStringLiteral("minLatitude")).toDouble();
        outPkg.coverage.maxLatitude = cov.value(QStringLiteral("maxLatitude")).toDouble();
        outPkg.coverage.minLongitude = cov.value(QStringLiteral("minLongitude")).toDouble();
        outPkg.coverage.maxLongitude = cov.value(QStringLiteral("maxLongitude")).toDouble();
    } else if (outPkg.regionId == QStringLiteral("uttarakhand")) {
        outPkg.coverage.minLatitude = 28.7;
        outPkg.coverage.maxLatitude = 31.5;
        outPkg.coverage.minLongitude = 77.5;
        outPkg.coverage.maxLongitude = 81.1;
    } else if (outPkg.regionId == QStringLiteral("sample-region")) {
        outPkg.coverage.minLatitude = 28.6290;
        outPkg.coverage.maxLatitude = 28.6365;
        outPkg.coverage.minLongitude = 77.2130;
        outPkg.coverage.maxLongitude = 77.2270;
    }

    if (!outPkg.coverage.isValid()) {
        outErrors.append(QStringLiteral("Invalid geographic coverage coordinates in manifest."));
    }

    // Relative file references
    const QString mapRel = root.value(QStringLiteral("renderingFile")).toString().trimmed();
    const QString graphRel = root.value(QStringLiteral("routingFile")).toString().trimmed();
    const QString boundaryRel = root.value(QStringLiteral("boundaryFile")).toString(QStringLiteral("boundary.geojson")).trimmed();
    const QString placesRel = root.value(QStringLiteral("placesFile")).toString(QStringLiteral("places.json")).trimmed();

    // Check path traversal
    if (!mapRel.isEmpty() && isPathTraversal(packageDir.absolutePath(), mapRel)) {
        outErrors.append(QStringLiteral("Path traversal detected in renderingFile: %1").arg(mapRel));
    }
    if (!graphRel.isEmpty() && isPathTraversal(packageDir.absolutePath(), graphRel)) {
        outErrors.append(QStringLiteral("Path traversal detected in routingFile: %1").arg(graphRel));
    }

    // Resolve file locations
    if (!mapRel.isEmpty()) {
        const QString mapAbs = packageDir.filePath(mapRel);
        if (QFile::exists(mapAbs)) {
            outPkg.mapFilePath = mapAbs;
            outPkg.hasRenderingData = true;
        }
    }
    if (!graphRel.isEmpty()) {
        const QString graphAbs = packageDir.filePath(graphRel);
        if (QFile::exists(graphAbs)) {
            outPkg.graphFilePath = graphAbs;
            outPkg.hasRoutingData = true;
        }
    }
    if (!boundaryRel.isEmpty()) {
        const QString boundAbs = packageDir.filePath(boundaryRel);
        if (QFile::exists(boundAbs)) {
            outPkg.boundaryFilePath = boundAbs;
        }
    }
    if (!placesRel.isEmpty()) {
        const QString placesAbs = packageDir.filePath(placesRel);
        if (QFile::exists(placesAbs)) {
            outPkg.placesFilePath = placesAbs;
        }
    }

    // Validate declared checksum if present
    const QString expectedSha = root.value(QStringLiteral("sha256Checksum")).toString(
        root.value(QStringLiteral("sha256")).toString()
    ).trimmed();
    outPkg.sha256Checksum = expectedSha;

    const QString sourceFileRel = root.value(QStringLiteral("sourceFile")).toString().trimmed();
    if (!sourceFileRel.isEmpty()) {
        const QString sourceAbs = packageDir.filePath(sourceFileRel);
        if (QFile::exists(sourceAbs)) {
            outPkg.fileSizeBytes = QFileInfo(sourceAbs).size();
            if (!expectedSha.isEmpty()) {
                QFile srcF(sourceAbs);
                if (srcF.open(QIODevice::ReadOnly)) {
                    QCryptographicHash hash(QCryptographicHash::Sha256);
                    if (hash.addData(&srcF)) {
                        const QString actualSha = QString::fromLatin1(hash.result().toHex());
                        if (actualSha.compare(expectedSha, Qt::CaseInsensitive) != 0) {
                            outErrors.append(QStringLiteral("Checksum mismatch for %1: expected %2, got %3")
                                .arg(sourceFileRel, expectedSha, actualSha));
                        }
                    }
                    srcF.close();
                }
            }
        }
    }

    // Check declared file size
    const qint64 declaredSize = root.value(QStringLiteral("fileSizeBytes")).toVariant().toLongLong();
    if (declaredSize > 0) {
        outPkg.fileSizeBytes = declaredSize;
    }

    outPkg.validationErrors = outErrors;
    outPkg.status = outErrors.isEmpty() ? PackageStatus::Installed : PackageStatus::Corrupt;
    return outErrors.isEmpty();
}

std::vector<MapPackage> MapPackageManager::discoverPackages(const QString &searchDir) {
    std::vector<MapPackage> packages;
    QDir dir(searchDir);
    if (!dir.exists()) return packages;

    // Scan subdirectories for manifest.json
    const QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &sub : subdirs) {
        const QString manifestPath = dir.filePath(sub + QStringLiteral("/manifest.json"));
        if (QFile::exists(manifestPath)) {
            MapPackage pkg;
            QStringList errors;
            validatePackageManifest(manifestPath, pkg, errors);
            packages.push_back(pkg);
        }
    }
    return packages;
}

bool MapPackageManager::copyPackageFiles(
    const MapPackage &pkg,
    const QString &srcDir,
    const QString &dstDir,
    QString &outError
) {
    QDir src(srcDir);
    QDir dst(dstDir);
    if (!dst.mkpath(QStringLiteral("."))) {
        outError = QStringLiteral("Failed to create destination directory: ") + dstDir;
        return false;
    }

    const QStringList files = src.entryList(QDir::Files);
    for (const QString &f : files) {
        const QString srcFile = src.filePath(f);
        const QString dstFile = dst.filePath(f);
        if (QFile::exists(dstFile)) {
            QFile::remove(dstFile);
        }
        if (!QFile::copy(srcFile, dstFile)) {
            outError = QStringLiteral("Failed to copy %1 to %2").arg(srcFile, dstFile);
            return false;
        }
    }
    return true;
}

bool MapPackageManager::installPackage(const QString &manifestPath, QString &outError) {
    outError.clear();

    // Stage 1: Validate manifest
    MapPackage pkg;
    QStringList errors;
    if (!validatePackageManifest(manifestPath, pkg, errors)) {
        outError = QStringLiteral("Manifest validation failed: ") + errors.join(QStringLiteral("; "));
        return false;
    }

    // Stage 2: Check Storage
    if (m_simulateStorageFull) {
        outError = QStringLiteral("Insufficient storage capacity to install package: ") + pkg.regionId;
        return false;
    }

    const QFileInfo manifestInfo(manifestPath);
    const QString srcDir = manifestInfo.absolutePath();

    // Stage 3: Staging directory
    QDir baseDir(m_baseInstallDir);
    baseDir.mkpath(QStringLiteral("."));
    const QString stagingDirName = QStringLiteral("staging_%1_%2")
        .arg(pkg.regionId)
        .arg(QDateTime::currentMSecsSinceEpoch());
    const QString stagingPath = baseDir.filePath(stagingDirName);

    if (!copyPackageFiles(pkg, srcDir, stagingPath, outError)) {
        QDir(stagingPath).removeRecursively();
        return false;
    }

    // Stage 4: Verify staged files
    const QString stagedManifest = QDir(stagingPath).filePath(QStringLiteral("manifest.json"));
    MapPackage stagedPkg;
    QStringList stagedErrors;
    if (!validatePackageManifest(stagedManifest, stagedPkg, stagedErrors)) {
        outError = QStringLiteral("Verification of staged files failed: ") + stagedErrors.join(QStringLiteral("; "));
        QDir(stagingPath).removeRecursively();
        return false;
    }

    // Stage 5: Atomic activation / replacement
    const QString targetDirName = QStringLiteral("installed_%1").arg(pkg.regionId);
    const QString targetPath = baseDir.filePath(targetDirName);
    const QString backupPath = baseDir.filePath(QStringLiteral("backup_%1").arg(pkg.regionId));

    if (QDir(targetPath).exists()) {
        if (QDir(backupPath).exists()) {
            QDir(backupPath).removeRecursively();
        }
        if (!QDir().rename(targetPath, backupPath)) {
            outError = QStringLiteral("Failed to create backup of existing installation.");
            QDir(stagingPath).removeRecursively();
            return false;
        }
    }

    if (!QDir().rename(stagingPath, targetPath)) {
        outError = QStringLiteral("Failed to activate staged package into target location.");
        // Rollback from backup if available
        if (QDir(backupPath).exists()) {
            QDir().rename(backupPath, targetPath);
        }
        QDir(stagingPath).removeRecursively();
        return false;
    }

    // Successfully installed - clean up backup
    if (QDir(backupPath).exists()) {
        QDir(backupPath).removeRecursively();
    }

    // Update paths to point to installed files
    QDir targetDir(targetPath);
    stagedPkg.mapFilePath = targetDir.filePath(QFileInfo(stagedPkg.mapFilePath).fileName());
    stagedPkg.graphFilePath = targetDir.filePath(QFileInfo(stagedPkg.graphFilePath).fileName());
    stagedPkg.placesFilePath = targetDir.filePath(QFileInfo(stagedPkg.placesFilePath).fileName());
    stagedPkg.boundaryFilePath = targetDir.filePath(QFileInfo(stagedPkg.boundaryFilePath).fileName());
    stagedPkg.installedAtUtcMs = QDateTime::currentMSecsSinceEpoch();
    stagedPkg.status = PackageStatus::Installed;

    if (m_repository) {
        m_repository->savePackage(stagedPkg);
    }

    emit packageInstalled(pkg.regionId);
    qInfo() << "[MapPackageManager] Successfully installed map package:" << pkg.regionId << "at" << targetPath;
    return true;
}

bool MapPackageManager::registerExistingPackage(const MapPackage &pkg) {
    if (!m_repository) return false;
    MapPackage copy = pkg;
    copy.installedAtUtcMs = QDateTime::currentMSecsSinceEpoch();
    copy.status = PackageStatus::Installed;
    return m_repository->savePackage(copy);
}

bool MapPackageManager::activateRegion(const QString &regionId) {
    if (!m_repository) return false;
    const bool ok = m_repository->setActivePackage(regionId);
    if (ok) {
        emit activeRegionChanged(regionId);
        qInfo() << "[MapPackageManager] Activated region:" << regionId;
    }
    return ok;
}

bool MapPackageManager::removePackage(const QString &regionId) {
    if (!m_repository) return false;
    const QString targetDirName = QStringLiteral("installed_%1").arg(regionId);
    const QString targetPath = QDir(m_baseInstallDir).filePath(targetDirName);
    if (QDir(targetPath).exists()) {
        QDir(targetPath).removeRecursively();
    }
    const bool ok = m_repository->deletePackage(regionId);
    if (ok) {
        emit packageRemoved(regionId);
    }
    return ok;
}

std::vector<MapPackage> MapPackageManager::listInstalledPackages() const {
    if (!m_repository) return {};
    return m_repository->listPackages();
}

std::optional<MapPackage> MapPackageManager::getPackage(const QString &regionId) const {
    if (!m_repository) return std::nullopt;
    return m_repository->getPackage(regionId);
}

std::optional<MapPackage> MapPackageManager::getActivePackage() const {
    if (!m_repository) return std::nullopt;
    return m_repository->getActivePackage();
}

bool MapPackageManager::isCoordinateCovered(double lat, double lon) const {
    auto active = getActivePackage();
    if (!active) return false;
    return active->isCoordinateInside(lat, lon);
}

bool MapPackageManager::isCoordinateInsideRegion(const QString &regionId, double lat, double lon) const {
    auto pkg = getPackage(regionId);
    if (!pkg) return false;
    return pkg->isCoordinateInside(lat, lon);
}

} // namespace georescux::domain
