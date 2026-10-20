#ifndef GEORESCUX_MAP_PACKAGE_MANAGER_H
#define GEORESCUX_MAP_PACKAGE_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include <optional>
#include "MapPackageModel.h"
#include "IMapPackageRepository.h"

namespace georescux::domain {

/**
 * @brief Manages the offline regional map package lifecycle:
 * DISCOVER -> VALIDATE -> CHECK STORAGE -> INSTALL -> VERIFY -> ACTIVATE.
 */
class MapPackageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeRegionId READ activeRegionId NOTIFY activeRegionChanged)
    Q_PROPERTY(QString activeRegionDisplayName READ activeRegionDisplayName NOTIFY activeRegionChanged)
    Q_PROPERTY(bool hasActivePackage READ hasActivePackage NOTIFY activeRegionChanged)
    Q_PROPERTY(bool activeHasRenderingData READ activeHasRenderingData NOTIFY activeRegionChanged)
    Q_PROPERTY(bool activeHasRoutingData READ activeHasRoutingData NOTIFY activeRegionChanged)

public:
    explicit MapPackageManager(
        std::shared_ptr<IMapPackageRepository> repository,
        QString baseInstallDir = QString(),
        QObject *parent = nullptr
    );
    ~MapPackageManager() override = default;

    [[nodiscard]] QString activeRegionId() const;
    [[nodiscard]] QString activeRegionDisplayName() const;
    [[nodiscard]] bool hasActivePackage() const;
    [[nodiscard]] bool activeHasRenderingData() const;
    [[nodiscard]] bool activeHasRoutingData() const;

    [[nodiscard]] QString baseInstallDirectory() const { return m_baseInstallDir; }
    void setBaseInstallDirectory(const QString &dir) { m_baseInstallDir = dir; }

    // Lifecycle Operations
    std::vector<MapPackage> discoverPackages(const QString &searchDirectory);
    bool validatePackageManifest(const QString &manifestPath, MapPackage &outPackage, QStringList &outErrors);
    bool installPackage(const QString &manifestPath, QString &outError);
    bool activateRegion(const QString &regionId);
    bool removePackage(const QString &regionId);

    // Query Operations
    std::vector<MapPackage> listInstalledPackages() const;
    std::optional<MapPackage> getPackage(const QString &regionId) const;
    std::optional<MapPackage> getActivePackage() const;
    bool isCoordinateCovered(double lat, double lon) const;
    bool isCoordinateInsideRegion(const QString &regionId, double lat, double lon) const;

    // Direct registration of bundled/in-situ package assets (e.g., from assets/ or output/)
    bool registerExistingPackage(const MapPackage &package);

    // Test seams
    void setSimulateStorageFull(bool full) { m_simulateStorageFull = full; }
    [[nodiscard]] bool isPathTraversal(const QString &baseDir, const QString &targetPath) const;

signals:
    void activeRegionChanged(const QString &regionId);
    void packageInstalled(const QString &regionId);
    void packageRemoved(const QString &regionId);
    void packageValidationError(const QString &regionId, const QString &error);

private:
    std::shared_ptr<IMapPackageRepository> m_repository;
    QString m_baseInstallDir;
    bool m_simulateStorageFull{false};

    bool copyPackageFiles(const MapPackage &pkg, const QString &srcDir, const QString &dstDir, QString &outError);
};

} // namespace georescux::domain

#endif // GEORESCUX_MAP_PACKAGE_MANAGER_H
