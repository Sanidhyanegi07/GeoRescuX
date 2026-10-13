#pragma once

#include <string>

namespace georescux::routing {

/**
 * A node of the offline evacuation road graph: an intersection or destination.
 * Safe havens are valid evacuation endpoints.
 */
struct RouteNode {
    std::string id;
    double latitude{0.0};
    double longitude{0.0};
    bool isSafeHaven{false};

    RouteNode() = default;
    RouteNode(std::string id, double lat, double lng, bool safeHaven = false)
        : id(std::move(id)), latitude(lat), longitude(lng), isSafeHaven(safeHaven) {}
};

} // namespace georescux::routing
