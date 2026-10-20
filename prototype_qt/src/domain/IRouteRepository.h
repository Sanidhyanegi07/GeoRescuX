#ifndef GEORESCUX_I_ROUTE_REPOSITORY_H
#define GEORESCUX_I_ROUTE_REPOSITORY_H

#include <QString>
#include <optional>
#include <memory>
#include "RouteGraph.h"

namespace georescux::domain {

/**
 * @brief Abstract repository contract for loading, caching, and querying regional road graphs.
 */
class IRouteRepository {
public:
    virtual ~IRouteRepository() = default;

    virtual std::optional<georescux::routing::RouteGraph> getGraph(const QString &regionId) = 0;
    virtual bool hasGraph(const QString &regionId) = 0;
    virtual bool saveGraph(const QString &regionId, const georescux::routing::RouteGraph &graph) = 0;
    virtual void clearCache() = 0;
};

} // namespace georescux::domain

#endif // GEORESCUX_I_ROUTE_REPOSITORY_H
