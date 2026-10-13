#pragma once

#include <optional>
#include <string>
#include "RouteGraph.h"
#include "EvacuationRoute.h"

namespace georescux::routing {

class SafeHavenFallback {
public:
    static std::optional<EvacuationRoute> findRoute(
        const RouteGraph& graph,
        const std::string& startId,
        const std::string& destinationId
    );
};

} // namespace georescux::routing
