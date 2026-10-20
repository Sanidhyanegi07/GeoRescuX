#ifndef GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H
#define GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H

#include <unordered_map>
#include <string>
#include <memory>
#include <QMutex>
#include "domain/IRouteRepository.h"
#include "domain/MapPackageManager.h"

namespace georescux::domain {

class OfflineRouteRepository : public IRouteRepository {
public:
    explicit OfflineRouteRepository(std::shared_ptr<MapPackageManager> packageManager = nullptr);
    ~OfflineRouteRepository() override = default;

    std::optional<georescux::routing::RouteGraph> getGraph(const QString &regionId) override;
    bool hasGraph(const QString &regionId) override;
    bool saveGraph(const QString &regionId, const georescux::routing::RouteGraph &graph) override;
    void clearCache() override;

    // Direct registration for tests or bundled seeds
    void setCustomGraphPath(const QString &regionId, const QString &filePath);
    void seedGraph(const QString &regionId, const georescux::routing::RouteGraph &graph);

private:
    std::shared_ptr<MapPackageManager> m_packageManager;
    mutable QMutex m_mutex;
    std::unordered_map<std::string, georescux::routing::RouteGraph> m_cache;
    std::unordered_map<std::string, std::string> m_customPaths;

    QString resolveGraphFilePath(const QString &regionId) const;
};

} // namespace georescux::domain

#endif // GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H
