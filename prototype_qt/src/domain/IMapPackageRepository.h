#ifndef GEORESCUX_I_MAP_PACKAGE_REPOSITORY_H
#define GEORESCUX_I_MAP_PACKAGE_REPOSITORY_H

#include <vector>
#include <optional>
#include "MapPackageModel.h"

namespace georescux::domain {

class IMapPackageRepository {
public:
    virtual ~IMapPackageRepository() = default;

    virtual bool savePackage(const MapPackage &package) = 0;
    virtual std::optional<MapPackage> getPackage(const QString &regionId) = 0;
    virtual std::vector<MapPackage> listPackages() = 0;
    virtual bool deletePackage(const QString &regionId) = 0;
    virtual std::optional<MapPackage> getActivePackage() = 0;
    virtual bool setActivePackage(const QString &regionId) = 0;
};

} // namespace georescux::domain

#endif // GEORESCUX_I_MAP_PACKAGE_REPOSITORY_H
