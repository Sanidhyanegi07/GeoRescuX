#ifndef GEORESCUX_I_LOCATION_PROVIDER_H
#define GEORESCUX_I_LOCATION_PROVIDER_H

#include <functional>
#include "LocationModel.h"

namespace georescux::domain {

/**
 * @brief Abstract platform contract for hardware location acquisition and permission negotiation.
 * Enables dependency injection of deterministic mock/fake providers in unit tests.
 */
class ILocationProvider {
public:
    virtual ~ILocationProvider() = default;

    [[nodiscard]] virtual LocationPermissionStatus permissionStatus() const = 0;
    virtual void requestPermission() = 0;

    [[nodiscard]] virtual bool isLocationServicesEnabled() const = 0;
    virtual void startUpdates(bool highFrequencyEmergency = false) = 0;
    virtual void stopUpdates() = 0;
    [[nodiscard]] virtual bool isUpdating() const = 0;

    virtual void setOnPositionChanged(std::function<void(const GeoPosition&)> callback) = 0;
    virtual void setOnStateChanged(std::function<void(LocationTrackingState)> callback) = 0;
    virtual void setOnPermissionChanged(std::function<void(LocationPermissionStatus)> callback) = 0;
};

} // namespace georescux::domain

#endif // GEORESCUX_I_LOCATION_PROVIDER_H
