#ifndef GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H
#define GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H

#include <unordered_map>
#include <string>
#include <memory>
#include <QMutex>
#include "domain/IRouteRepository.h"
#include "domain/MapPackageManager.h"

namespace georescux::storage {

/**
 * @brief Zero-network, offline-first implementation of IRouteRepository.
 *
 * Resolves regional road network JSON graphs from application-private storage,
 * bundled assets (assets:/ on Android), or in-situ repository paths.
 */
class OfflineRouteRepository : public georescux::domain::IRouteRepository {
public:
    explicit OfflineRouteRepository(std::shared_ptr<georescux::domain::MapPackageManager> packageManager = nullptr);
    ~OfflineRouteRepository() override = default;

    std::optional<georescux::routing::RouteGraph> getGraph(const QString &regionId) override;
    bool hasGraph(const QString &regionId) override;
    bool saveGraph(const QString &regionId, const georescux::routing::RouteGraph &graph) override;
    void clearCache() override;

    // Direct registration for tests or bundled seeds
    void setCustomGraphPath(const QString &regionId, const QString &filePath);
    void seedGraph(const QString &regionId, const georescux::routing::RouteGraph &graph);

private:
    std::shared_ptr<georescux::domain::MapPackageManager> m_packageManager;
    mutable QMutex m_mutex;
    std::unordered_map<std::string, georescux::routing::RouteGraph> m_cache;
    std::unordered_map<std::string, std::string> m_customPaths;

    QString resolveGraphFilePath(const QString &regionId) const;
};

} // namespace georescux::storage

namespace georescux::domain {
    // Backward-compatibility alias for domain callers
    using OfflineRouteRepository = georescux::storage::OfflineRouteRepository;
} // namespace georescux::domain

#endif // GEORESCUX_OFFLINE_ROUTE_REPOSITORY_H
