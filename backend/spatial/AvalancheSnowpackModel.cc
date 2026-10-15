#include "AvalancheSnowpackModel.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
} // namespace

AvalancheSnowpackModel::AvalancheSnowpackModel(double gravity) : g_(gravity) {}

AvalancheHazardAssessment AvalancheSnowpackModel::evaluateSlabRelease(
    const std::vector<SnowLayer>& layers,
    double slopeAngleDegrees,
    double newSnow24hCm,
    double windSpeedKmh
) const noexcept {
    AvalancheHazardAssessment result{};
    result.dangerLevel = AvalancheDangerLevel::LOW;
    result.stabilityIndex = 3.0;
    result.criticalCrackLengthMeters = 50.0;
    result.estimatedPeakVelocityMps = 0.0;
    result.maximumRunoutDistanceMeters = 0.0;
    result.impactPressureKPa = 0.0;
    result.isTerrainTrap = false;

    // Slopes less than 25 degrees rarely produce slab avalanches; slopes > 55 deg sluff continuously
    if (layers.empty() || slopeAngleDegrees < 25.0 || slopeAngleDegrees > 60.0) {
        return result;
    }

    double slopeRad = slopeAngleDegrees * DEG2RAD;
    double sinSlope = std::sin(slopeRad);
    double cosSlope = std::cos(slopeRad);

    // Calculate overburden load of slab
    double totalMassPerM2 = 0.0;
    double totalDepth = 0.0;

    for (const auto& layer : layers) {
        totalMassPerM2 += layer.densityKgPerM3 * layer.depthMeters;
        totalDepth += layer.depthMeters;
    }

    // Additional wind drifted snow on leeward slope
    double windDriftFactor = 1.0;
    if (windSpeedKmh > 20.0) {
        windDriftFactor += (windSpeedKmh - 20.0) * 0.025; // snow transport increases nonlinearly with wind
    }
    double newSnowMassPerM2 = (newSnow24hCm * 0.01) * 120.0 * windDriftFactor;
    totalMassPerM2 += newSnowMassPerM2;

    double normalStressTotalKPa = (totalMassPerM2 * g_ * cosSlope * cosSlope) / 1000.0;
    double shearStressTotalKPa = (totalMassPerM2 * g_ * sinSlope * cosSlope) / 1000.0;

    // Search for weakest interface
    double minStability = 999.0;
    double weakLayerDepth = 0.0;
    double weakLayerDensity = 250.0;
    double weakLayerStrength = 10.0;
    bool foundWeakLayer = false;

    double cumDepth = 0.0;
    for (const auto& layer : layers) {
        cumDepth += layer.depthMeters;
        double layerStrength = layer.shearStrengthKPa;

        // Persistent weak layers (surface hoar, depth hoar) have severely impaired shear strength
        if (layer.isPersistentWeakLayer) {
            layerStrength = std::min(layerStrength, 1.2);
            foundWeakLayer = true;
        }

        double localShear = shearStressTotalKPa * (cumDepth / std::max(0.01, totalDepth));
        if (localShear > 0.05) {
            double s = layerStrength / localShear;
            if (s < minStability) {
                minStability = s;
                weakLayerDepth = cumDepth;
                weakLayerDensity = layer.densityKgPerM3;
                weakLayerStrength = layerStrength;
            }
        }
    }

    result.stabilityIndex = minStability;

    // McClung critical crack length for slab release: rc = sqrt( (2 * E' * Gc) / (pi * tau^2) )
    double avgDensity = (totalDepth > 0.01) ? (totalMassPerM2 / totalDepth) : 250.0;
    double youngsModulusPa = 2.5e5 * std::pow(avgDensity / 300.0, 3.0);
    double fractureEnergyGc = 0.5 * std::pow(weakLayerStrength * 1000.0, 2.0) * (totalDepth / youngsModulusPa);
    fractureEnergyGc = std::clamp(fractureEnergyGc, 0.05, 5.0); // J/m2

    double tauPa = shearStressTotalKPa * 1000.0;
    if (tauPa > 10.0) {
        double underSqrt = (2.0 * youngsModulusPa * fractureEnergyGc) / (PI * tauPa * tauPa);
        result.criticalCrackLengthMeters = std::sqrt(std::max(0.1, underSqrt));
    }

    // Determine Danger Level
    if (minStability < 0.85 || (minStability < 1.05 && foundWeakLayer)) {
        result.dangerLevel = (newSnow24hCm > 30.0 || windSpeedKmh > 50.0) ? 
                             AvalancheDangerLevel::VERY_HIGH : AvalancheDangerLevel::HIGH;
    } else if (minStability < 1.30) {
        result.dangerLevel = AvalancheDangerLevel::CONSIDERABLE;
    } else if (minStability < 1.75) {
        result.dangerLevel = AvalancheDangerLevel::MODERATE;
    } else {
        result.dangerLevel = AvalancheDangerLevel::LOW;
    }

    // Voellmy-Salm peak flow velocity: v = sqrt(xi * d * (sin theta - mu * cos theta))
    double mu = 0.16; // dry friction
    double xi = 1500.0; // turbulent friction
    double flowDepthD = std::max(0.5, weakLayerDepth * 0.7);

    double drivingDiff = sinSlope - mu * cosSlope;
    if (drivingDiff > 0.0) {
        result.estimatedPeakVelocityMps = std::sqrt(xi * flowDepthD * drivingDiff);
        result.maximumRunoutDistanceMeters = estimateRunoutDistance(
            totalDepth * 10000.0, slopeAngleDegrees, 12.0, mu, xi
        );
        result.impactPressureKPa = calculateDynamicImpactPressureKPa(result.estimatedPeakVelocityMps, 300.0);
    }

    // Slopes between 35 and 45 degrees with gullies are classic terrain traps
    if (slopeAngleDegrees >= 34.0 && slopeAngleDegrees <= 45.0) {
        result.isTerrainTrap = true;
    }

    return result;
}

double AvalancheSnowpackModel::calculateDynamicImpactPressureKPa(
    double flowVelocityMps,
    double snowFlowDensityKgPerM3
) const noexcept {
    // Dynamic ram pressure: p = 0.5 * Cd * rho * v^2
    // Drag coefficient Cd ~ 1.5 for structures
    constexpr double CD = 1.5;
    double pressurePa = 0.5 * CD * snowFlowDensityKgPerM3 * flowVelocityMps * flowVelocityMps;
    return pressurePa / 1000.0; // kPa
}

double AvalancheSnowpackModel::estimateRunoutDistance(
    double releaseVolumeM3,
    double trackSlopeDeg,
    double runoutSlopeDeg,
    double frictionMu,
    double turbulentXi
) const noexcept {
    double trackRad = trackSlopeDeg * DEG2RAD;
    double runoutRad = runoutSlopeDeg * DEG2RAD;

    double sinTrack = std::sin(trackRad);
    double cosTrack = std::cos(trackRad);
    double sinRunout = std::sin(runoutRad);
    double cosRunout = std::cos(runoutRad);

    double drivingTrack = sinTrack - frictionMu * cosTrack;
    if (drivingTrack <= 0.0) return 0.0;

    double flowDepth = std::clamp(std::cbrt(releaseVolumeM3) * 0.25, 0.5, 4.0);
    double vTrack = std::sqrt(turbulentXi * flowDepth * drivingTrack);

    // Deceleration in runout zone: a = g * (mu * cosRunout - sinRunout) + (g / (xi * d)) * v^2
    double brakingDecel = g_ * (frictionMu * cosRunout - sinRunout);
    if (brakingDecel <= 0.1) brakingDecel = 0.1;

    // Runout distance: s = v^2 / (2 * a)
    double runoutMeters = (vTrack * vTrack) / (2.0 * brakingDecel);
    return std::clamp(runoutMeters, 50.0, 3500.0);
}

} // namespace georescux::backend::spatial
