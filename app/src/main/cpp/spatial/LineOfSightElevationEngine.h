#ifndef GEORESCUX_LINE_OF_SIGHT_ELEVATION_ENGINE_H
#define GEORESCUX_LINE_OF_SIGHT_ELEVATION_ENGINE_H

#include <vector>
#include <string>
#include <cmath>
#include "DigitalElevationModel.h"

namespace georescux::spatial {

struct RfAntennaConfig {
    double latitude;
    double longitude;
    double towerHeightAboveGroundMeters;
    double frequencyMhz; // e.g. 2400 MHz (BLE), 868 MHz (LoRa), 433 MHz (VHF)
    double transmitPowerDbm;
    double receiverSensitivityDbm;
};

struct TerrainObstructionPoint {
    double latitude;
    double longitude;
    double distanceAlongRayMeters;
    double terrainElevationMeters;
    double rayHeightMeters;
    double fresnelRadiusMeters;
    double clearanceFraction; // < 0.6 indicates Fresnel zone diffraction loss
};

struct RfPropagationResult {
    bool hasDirectLineOfSight;
    bool hasFresnelClearance; // >= 60% of 1st Fresnel zone unobstructed
    double totalDistanceMeters;
    double freeSpacePathLossDb;
    double diffractionLossDb;
    double totalPathLossDb;
    double estimatedRssiDbm;
    bool isLinkViable;
    std::vector<TerrainObstructionPoint> profileSamples;
};

/**
 * RF Line-of-Sight and Fresnel Zone Ray-Marching Engine.
 * Evaluates wireless mesh propagation across Himalayan valleys and ridges,
 * calculating knife-edge diffraction losses (Deygout method) and Fresnel clearance.
 */
class LineOfSightElevationEngine {
public:
    LineOfSightElevationEngine(const DigitalElevationModel& dem);

    /**
     * Evaluates radio link feasibility between transmitter and receiver across terrain.
     * @param tx Transmitter configuration
     * @param rx Receiver configuration
     * @param sampleStepMeters Distance between terrain elevation sampling points (e.g. 10m)
     */
    [[nodiscard]] RfPropagationResult evaluateRadioLink(
        const RfAntennaConfig& tx,
        const RfAntennaConfig& rx,
        double sampleStepMeters = 15.0
    ) const;

    /**
     * Calculates 1st Fresnel zone radius at distance d1 from TX and d2 from RX.
     */
    [[nodiscard]] static double calculateFresnelRadiusMeters(
        double d1Meters,
        double d2Meters,
        double totalDistanceMeters,
        double frequencyMhz
    ) noexcept;

    /**
     * Calculates Free Space Path Loss (FSPL) in dB: FSPL = 20*log10(d) + 20*log10(f) - 27.55
     */
    [[nodiscard]] static double calculateFsplDb(double distanceMeters, double frequencyMhz) noexcept;

private:
    const DigitalElevationModel& dem_;
};

} // namespace georescux::spatial

#endif // GEORESCUX_LINE_OF_SIGHT_ELEVATION_ENGINE_H
