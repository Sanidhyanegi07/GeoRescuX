#ifndef GEORESCUX_BACKEND_LANDSLIDE_SUSCEPTIBILITY_ENGINE_H
#define GEORESCUX_BACKEND_LANDSLIDE_SUSCEPTIBILITY_ENGINE_H

#include <vector>
#include <string>
#include <cmath>

namespace georescux::backend::spatial {

enum class SlopeStabilityCategory {
    STABLE,              // FS > 1.30
    MARGINALLY_STABLE,   // 1.00 < FS <= 1.30
    HIGH_IMMINENT_RISK,  // 0.85 < FS <= 1.00
    ACTIVE_SLIP_FAILURE  // FS <= 0.85
};

struct SoilRockProperties {
    double effectiveCohesionKPa;     // c' (e.g. 5 - 25 kPa)
    double internalFrictionAngleDeg; // phi' (e.g. 28 - 38 degrees)
    double saturatedUnitWeightKNm3;  // gamma_sat (e.g. 19.5 kN/m3)
    double hydraulicDiffusivityM2s;  // D_0 (e.g. 1e-4 to 1e-3 m2/s)
    double soilRegolithDepthMeters;  // z (depth to bedrock / slip surface)
};

struct RainfallEvent {
    double intensityMmPerHour;
    double durationHours;
    double cumulativePreceding7DaysMm; // Antecedent Soil Moisture Index
};

struct LandslideRiskAssessment {
    SlopeStabilityCategory category;
    double factorOfSafety;
    double poreWaterPressureKPa;
    double criticalRainfallThresholdMmPerHour;
    double estimatedDebrisVelocityMps;
    double expectedRunoutDistanceMeters;
    bool isHighwaySeveranceExpected;
};

/**
 * Geotechnical Infinite Slope Stability and Iverson Transient Pore-Pressure Engine.
 * Predicts monsoon-induced landslides, rockfalls, and debris flow runouts
 * across steep Himalayan road cuts (NH-58, NH-107, NH-125) in Uttarakhand.
 */
class LandslideSusceptibilityEngine {
public:
    LandslideSusceptibilityEngine(double gravity = 9.80665);

    /**
     * Evaluates slope stability and calculates Factor of Safety (FS).
     */
    [[nodiscard]] LandslideRiskAssessment evaluateSlopeStability(
        double slopeAngleDegrees,
        const SoilRockProperties& soil,
        const RainfallEvent& rainfall,
        bool isRoadCutUndercut = false
    ) const noexcept;

    /**
     * Calculates steady-state critical rainfall intensity threshold for slope failure (FS = 1.0).
     */
    [[nodiscard]] double calculateCriticalRainfallThreshold(
        double slopeAngleDegrees,
        const SoilRockProperties& soil
    ) const noexcept;

    /**
     * Estimates debris flow travel distance and deposition zone.
     */
    [[nodiscard]] double estimateDebrisRunoutDistance(
        double dropHeightMeters,
        double slopeAngleDegrees,
        double debrisVolumeM3
    ) const noexcept;

private:
    double g_;
    [[nodiscard]] double computeTransientPorePressure(
        const SoilRockProperties& soil,
        const RainfallEvent& rainfall,
        double slopeAngleDegrees
    ) const noexcept;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_LANDSLIDE_SUSCEPTIBILITY_ENGINE_H
