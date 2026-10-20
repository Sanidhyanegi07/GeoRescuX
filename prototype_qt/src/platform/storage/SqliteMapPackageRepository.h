#ifndef GEORESCUX_SQLITE_MAP_PACKAGE_REPOSITORY_H
#define GEORESCUX_SQLITE_MAP_PACKAGE_REPOSITORY_H

#include <memory>
#include "domain/IMapPackageRepository.h"
#include "platform/storage/LocalDatabase.h"

namespace georescux::storage {

class SqliteMapPackageRepository : public georescux::domain::IMapPackageRepository {
public:
    explicit SqliteMapPackageRepository(std::shared_ptr<LocalDatabase> database);
    ~SqliteMapPackageRepository() override = default;

    bool savePackage(const georescux::domain::MapPackage &package) override;
    std::optional<georescux::domain::MapPackage> getPackage(const QString &regionId) override;
    std::vector<georescux::domain::MapPackage> listPackages() override;
    bool deletePackage(const QString &regionId) override;
    std::optional<georescux::domain::MapPackage> getActivePackage() override;
    bool setActivePackage(const QString &regionId) override;

    bool ensureTableCreated();

private:
    std::shared_ptr<LocalDatabase> m_database;
    bool m_tableEnsured{false};
};

} // namespace georescux::storage

#endif // GEORESCUX_SQLITE_MAP_PACKAGE_REPOSITORY_H
