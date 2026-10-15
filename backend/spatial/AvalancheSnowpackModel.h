#ifndef GEORESCUX_BACKEND_AVALANCHE_SNOWPACK_MODEL_H
#define GEORESCUX_BACKEND_AVALANCHE_SNOWPACK_MODEL_H

#include <vector>
#include <string>
#include <cmath>

namespace georescux::backend::spatial {

enum class AvalancheDangerLevel {
    LOW = 1,        // Stable snowpack
    MODERATE = 2,   // Heightened risk on specific steep slopes
    CONSIDERABLE = 3, // Natural avalanches possible, human trigger very likely
    HIGH = 4,       // Widespread spontaneous slab releases
    VERY_HIGH = 5   // Catastrophic large-scale valley-wide avalanches
};

struct SnowLayer {
    double depthMeters;
    double densityKgPerM3;       // 100 kg/m3 (fresh powder) to 450 kg/m3 (wind slab/refrozen)
    double temperatureCelsius;
    double shearStrengthKPa;     // Weak layer shear resistance
    bool isPersistentWeakLayer;  // Surface hoar, depth hoar, or faceted crystals
};

struct AvalancheHazardAssessment {
    AvalancheDangerLevel dangerLevel;
    double stabilityIndex;         // S = tau_c / tau (< 1.0 unstable)
    double criticalCrackLengthMeters;
    double estimatedPeakVelocityMps;
    double maximumRunoutDistanceMeters;
    double impactPressureKPa;      // Dynamic ram pressure on buildings/shelters
    bool isTerrainTrap;           // Gully, crevasse, cliff drop
};

/**
 * McClung-Schaerer Slab Avalanche and Voellmy-Salm Runout Mathematical Model.
 * Evaluates high-altitude Himalayan snowpack mechanics, shear fracture energy,
 * and avalanche impact pressures across mountain rescue corridors.
 */
class AvalancheSnowpackModel {
public:
    AvalancheSnowpackModel(double gravity = 9.80665);

    /**
     * Evaluates slab release probability and avalanche metrics for a given snowpack and slope.
     * @param layers Stratigraphic snow layers from top to ground
     * @param slopeAngleDegrees Incline of slope
     * @param newSnow24hCm New snowfall accumulation in past 24 hours
     * @param windSpeedKmh Ridgetop wind speed driving lee-slope snow loading
     */
    [[nodiscard]] AvalancheHazardAssessment evaluateSlabRelease(
        const std::vector<SnowLayer>& layers,
        double slopeAngleDegrees,
        double newSnow24hCm = 0.0,
        double windSpeedKmh = 0.0
    ) const noexcept;

    /**
     * Estimates dynamic impact pressure (kPa) using Voellmy-Salm formulation.
     */
    [[nodiscard]] double calculateDynamicImpactPressureKPa(
        double flowVelocityMps,
        double snowFlowDensityKgPerM3 = 300.0
    ) const noexcept;

    /**
     * Estimates avalanche runout length along deceleration zone using Voellmy friction parameters.
     */
    [[nodiscard]] double estimateRunoutDistance(
        double releaseVolumeM3,
        double trackSlopeDeg,
        double runoutSlopeDeg,
        double frictionMu = 0.16,
        double turbulentXi = 1500.0
    ) const noexcept;

private:
    double g_;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_AVALANCHE_SNOWPACK_MODEL_H
