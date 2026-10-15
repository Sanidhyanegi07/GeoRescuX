#include "LandslideSusceptibilityEngine.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double GAMMA_WATER = 9.80665; // kN/m3
} // namespace

LandslideSusceptibilityEngine::LandslideSusceptibilityEngine(double gravity) : g_(gravity) {}

double LandslideSusceptibilityEngine::computeTransientPorePressure(
    const SoilRockProperties& soil,
    const RainfallEvent& rainfall,
    double slopeAngleDegrees
) const noexcept {
    double betaRad = slopeAngleDegrees * DEG2RAD;
    double cosBeta = std::cos(betaRad);
    double z = std::max(0.2, soil.soilRegolithDepthMeters);

    // Antecedent soil saturation factor (0.1 to 1.0)
    double s0 = std::clamp(rainfall.cumulativePreceding7DaysMm / 150.0, 0.1, 1.0);

    // Characteristic hydraulic diffusion time td = z^2 / (4 * D0)
    double d0 = std::max(1e-5, soil.hydraulicDiffusivityM2s);
    double tdSec = (z * z) / (4.0 * d0);

    double stormDurationSec = rainfall.durationHours * 3600.0;
    double timeRatio = stormDurationSec / tdSec;

    // Transient saturation wave response
    double saturationIncrease = (1.0 - s0) * (1.0 - std::exp(-timeRatio));
    double effectiveSaturation = s0 + saturationIncrease;

    // Additional dynamic head from heavy rainfall intensity (> 25 mm/h generates perched water table)
    double perchedWaterHead = 0.0;
    if (rainfall.intensityMmPerHour > 15.0) {
        perchedWaterHead = (rainfall.intensityMmPerHour - 15.0) * 0.02 * z;
    }

    double totalPoreWaterHead = (z * effectiveSaturation + perchedWaterHead);
    totalPoreWaterHead = std::min(totalPoreWaterHead, z * 1.2); // max pore pressure limited by regolith depth

    // Pore water pressure u = gamma_w * head * cos^2(beta)
    return GAMMA_WATER * totalPoreWaterHead * cosBeta * cosBeta;
}

LandslideRiskAssessment LandslideSusceptibilityEngine::evaluateSlopeStability(
    double slopeAngleDegrees,
    const SoilRockProperties& soil,
    const RainfallEvent& rainfall,
    bool isRoadCutUndercut
) const noexcept {
    LandslideRiskAssessment assessment{};
    assessment.category = SlopeStabilityCategory::STABLE;
    assessment.factorOfSafety = 3.0;
    assessment.poreWaterPressureKPa = 0.0;
    assessment.criticalRainfallThresholdMmPerHour = 0.0;
    assessment.estimatedDebrisVelocityMps = 0.0;
    assessment.expectedRunoutDistanceMeters = 0.0;
    assessment.isHighwaySeveranceExpected = false;

    // Very gentle slopes (< 15 deg) do not slide
    if (slopeAngleDegrees < 15.0) {
        return assessment;
    }

    double betaRad = slopeAngleDegrees * DEG2RAD;
    double phiRad = soil.internalFrictionAngleDeg * DEG2RAD;
    double sinBeta = std::sin(betaRad);
    double cosBeta = std::cos(betaRad);
    double tanPhi = std::tan(phiRad);

    double z = std::max(0.2, soil.soilRegolithDepthMeters);
    double gammaSat = std::max(12.0, soil.saturatedUnitWeightKNm3);
    double cohesion = soil.effectiveCohesionKPa;

    // Roadcut undercut penalty: reduces effective cohesion by 30% due to toe removal
    if (isRoadCutUndercut) {
        cohesion *= 0.70;
    }

    // Compute transient pore pressure
    double u = computeTransientPorePressure(soil, rainfall, slopeAngleDegrees);
    assessment.poreWaterPressureKPa = u;

    // Infinite Slope Stability Formula:
    // FS = [ c' + (gamma_sat * z * cos^2(beta) - u) * tan(phi') ] / [ gamma_sat * z * sin(beta) * cos(beta) ]
    double drivingStress = gammaSat * z * sinBeta * cosBeta;
    if (drivingStress <= 1e-4) {
        assessment.factorOfSafety = 10.0;
        return assessment;
    }

    double effectiveNormalStress = std::max(0.0, gammaSat * z * cosBeta * cosBeta - u);
    double resistingStress = cohesion + effectiveNormalStress * tanPhi;

    double fs = resistingStress / drivingStress;
    assessment.factorOfSafety = fs;

    // Categorize risk
    if (fs <= 0.85) {
        assessment.category = SlopeStabilityCategory::ACTIVE_SLIP_FAILURE;
    } else if (fs <= 1.00) {
        assessment.category = SlopeStabilityCategory::HIGH_IMMINENT_RISK;
    } else if (fs <= 1.30) {
        assessment.category = SlopeStabilityCategory::MARGINALLY_STABLE;
    } else {
        assessment.category = SlopeStabilityCategory::STABLE;
    }

    // Estimate debris flow velocity and runout if failure occurs
    if (fs <= 1.05) {
        double dropHeight = z + z * std::sin(betaRad) * 10.0;
        double excessSlope = std::tan(betaRad) - tanPhi;
        if (excessSlope > 0.0) {
            double v2 = 2.0 * g_ * dropHeight * (1.0 - (tanPhi / std::tan(betaRad)));
            assessment.estimatedDebrisVelocityMps = std::sqrt(std::max(0.0, v2));
        }

        double debrisVol = z * 250.0; // typical initial slide area ~ 250 m2
        assessment.expectedRunoutDistanceMeters = estimateDebrisRunoutDistance(dropHeight, slopeAngleDegrees, debrisVol);
        assessment.isHighwaySeveranceExpected = (assessment.expectedRunoutDistanceMeters > 30.0);
    }

    assessment.criticalRainfallThresholdMmPerHour = calculateCriticalRainfallThreshold(slopeAngleDegrees, soil);
    return assessment;
}

double LandslideSusceptibilityEngine::calculateCriticalRainfallThreshold(
    double slopeAngleDegrees,
    const SoilRockProperties& soil
) const noexcept {
    double betaRad = slopeAngleDegrees * DEG2RAD;
    double phiRad = soil.internalFrictionAngleDeg * DEG2RAD;
    double sinBeta = std::sin(betaRad);
    double cosBeta = std::cos(betaRad);
    double tanPhi = std::tan(phiRad);

    double z = std::max(0.2, soil.soilRegolithDepthMeters);
    double gammaSat = soil.saturatedUnitWeightKNm3;
    double cohesion = soil.effectiveCohesionKPa;

    // Threshold where FS = 1.0:
    // drivingStress = cohesion + (gamma_sat * z * cos^2(beta) - u_crit) * tan(phi)
    double driving = gammaSat * z * sinBeta * cosBeta;
    if (tanPhi <= 1e-4) return 0.0;

    double neededResistingMinusCohesion = (driving - cohesion) / tanPhi;
    double uCrit = (gammaSat * z * cosBeta * cosBeta) - neededResistingMinusCohesion;

    if (uCrit <= 0.0) {
        return 0.0; // Slope naturally fails even when completely dry!
    }

    // Invert pore pressure to rainfall intensity
    double maxHead = uCrit / (GAMMA_WATER * cosBeta * cosBeta);
    double critIntensity = (maxHead / z) * 35.0; // empirical relation for Uttarakhand granitic/metamorphic soils
    return std::clamp(critIntensity, 5.0, 150.0);
}

double LandslideSusceptibilityEngine::estimateDebrisRunoutDistance(
    double dropHeightMeters,
    double slopeAngleDegrees,
    double debrisVolumeM3
) const noexcept {
    // Corominas (1996) Fahrböschung empirical travel angle:
    // tan(alpha) = 0.94 * V^(-0.16) for unobstructed mountain slopes
    double vol = std::max(10.0, debrisVolumeM3);
    double tanAlpha = 0.94 * std::pow(vol, -0.16);
    tanAlpha = std::clamp(tanAlpha, 0.25, 0.85);

    double h = std::max(5.0, dropHeightMeters);
    double runout = h / tanAlpha;
    return std::clamp(runout, 20.0, 1500.0);
}

} // namespace georescux::backend::spatial
