#include "CasualtySurvivalProbability.h"
#include <algorithm>
#include <cmath>

namespace georescux::analytics {

double CasualtySurvivalProbability::estimateCoreTemperatureCelsius(
    double initialCoreTempC,
    double ambientTempC,
    double durationHours,
    double cloInsulation,
    bool isWet
) noexcept {
    // Human body parameters
    constexpr double BODY_MASS_KG = 70.0;
    constexpr double SPECIFIC_HEAT_J_PER_KG_C = 3470.0; // 3.47 kJ/kg C
    constexpr double BODY_SURFACE_AREA_M2 = 1.8;
    constexpr double BASAL_METABOLIC_RATE_WATTS = 75.0; // Resting shivering / basal metabolic rate

    double totalHeatCapacityJoules = BODY_MASS_KG * SPECIFIC_HEAT_J_PER_KG_C;

    // 1 clo = 0.155 m2 K / W
    double effectiveClo = std::max(0.2, cloInsulation);
    if (isWet) {
        // Wet clothes lose up to 85% of thermal resistance
        effectiveClo *= 0.15;
    }
    double thermalResistance = effectiveClo * 0.155; // (m2 K) / W

    double currentCoreTemp = initialCoreTempC;
    double dtSec = 60.0; // 1-minute integration step
    double totalSec = durationHours * 3600.0;

    for (double t = 0; t < totalSec; t += dtSec) {
        if (currentCoreTemp <= 24.0) {
            currentCoreTemp = 24.0; // minimum biological survival threshold
            break;
        }

        double deltaT = currentCoreTemp - ambientTempC;
        double heatLossWatts = (BODY_SURFACE_AREA_M2 * deltaT) / thermalResistance;

        // Shivering increases metabolic rate down to 32C, then shiver stops (exhaustion)
        double metabolicWatts = BASAL_METABOLIC_RATE_WATTS;
        if (currentCoreTemp > 32.0 && currentCoreTemp < 36.0) {
            metabolicWatts = 200.0; // vigorous shivering
        } else if (currentCoreTemp <= 30.0) {
            metabolicWatts = 40.0; // severe metabolic depression
        }

        double netPowerWatts = metabolicWatts - heatLossWatts;
        double dTemp = (netPowerWatts * dtSec) / totalHeatCapacityJoules;
        currentCoreTemp += dTemp;
    }

    return std::clamp(currentCoreTemp, 20.0, 42.0);
}

double CasualtySurvivalProbability::calculateAvalancheBurialSurvival(
    double burialDurationMinutes,
    bool hasAirPocket
) noexcept {
    if (burialDurationMinutes <= 0.0) return 1.0;

    // Falk & Brugger (1994) Swiss Avalanche Mortality Curve
    if (burialDurationMinutes <= 15.0) {
        // Survival Phase: 93% survival at 15 minutes
        return 1.0 - (0.07 * (burialDurationMinutes / 15.0));
    } else if (burialDurationMinutes <= 35.0) {
        // Asphyxiation Phase: Precipitous drop from 93% to 34%
        double fraction = (burialDurationMinutes - 15.0) / 20.0;
        return 0.93 - (0.59 * fraction);
    } else if (burialDurationMinutes <= 90.0) {
        // Latent Phase: Dependent entirely on air pocket
        if (hasAirPocket) {
            // Air pocket sustains life, plateauing around 25-30%
            double fraction = (burialDurationMinutes - 35.0) / 55.0;
            return 0.34 - (0.09 * fraction);
        } else {
            // No air pocket: asphyxiation completes
            double fraction = (burialDurationMinutes - 35.0) / 55.0;
            return std::max(0.01, 0.34 - (0.33 * fraction));
        }
    } else {
        // Hypothermia Phase (> 90 minutes)
        if (!hasAirPocket) return 0.005;
        double hoursPost90 = (burialDurationMinutes - 90.0) / 60.0;
        return std::max(0.001, 0.25 * std::exp(-0.45 * hoursPost90));
    }
}

SurvivalEstimate CasualtySurvivalProbability::computeSurvivalProbability(
    const CasualtyProfile& casualty,
    EntrapmentEnvironment env
) noexcept {
    SurvivalEstimate estimate{};
    estimate.overallSurvivalProbability = 1.0;
    estimate.estimatedCoreBodyTempCelsius = 37.0;
    estimate.remainingViableRescueHours = 72.0;
    estimate.isImmediateTriagePriority = false;
    estimate.primaryLethalityThreat = "None";

    // 1. Severe Bleeding Check (Fastest killer: 1-3 hours)
    if (casualty.hasSevereBleeding) {
        double bleedSurvival = std::exp(-1.2 * casualty.trappedDurationHours);
        estimate.overallSurvivalProbability *= bleedSurvival;
        estimate.primaryLethalityThreat = "Exsanguination";
        estimate.remainingViableRescueHours = std::max(0.1, 2.5 - casualty.trappedDurationHours);
        estimate.isImmediateTriagePriority = true;
    }

    // 2. Environment-Specific Mechanics
    if (env == EntrapmentEnvironment::SNOW_AVALANCHE_BURIAL) {
        double burialMins = casualty.trappedDurationHours * 60.0;
        double avSurvival = calculateAvalancheBurialSurvival(burialMins, casualty.hasConfirmedAirPocket);
        estimate.overallSurvivalProbability *= avSurvival;

        if (burialMins > 15.0 && !casualty.hasConfirmedAirPocket) {
            estimate.primaryLethalityThreat = "Asphyxiation";
            estimate.remainingViableRescueHours = std::max(0.1, (35.0 - burialMins) / 60.0);
        } else {
            estimate.primaryLethalityThreat = "Hypothermia";
        }
    } else if (env == EntrapmentEnvironment::CONCRETE_RUBBLE_COLLAPSE) {
        // Macintyre Weibull rubble survival curve: S(t) = exp(-(t / 48)^1.6)
        double t = casualty.trappedDurationHours;
        double rubbleSurvival = std::exp(-std::pow(t / 48.0, 1.6));
        estimate.overallSurvivalProbability *= rubbleSurvival;

        if (casualty.hasCrushSyndromeRisk && t > 4.0) {
            // Crush syndrome toxicity upon release
            estimate.overallSurvivalProbability *= 0.65;
            estimate.primaryLethalityThreat = "Crush Ischemia";
        }
    } else {
        // Open exposure / Mudslide
        double t = casualty.trappedDurationHours;
        double baseSurvival = std::exp(-std::pow(t / 36.0, 1.4));
        estimate.overallSurvivalProbability *= baseSurvival;
    }

    // 3. Hypothermia Modeling
    double coreTemp = estimateCoreTemperatureCelsius(
        37.0,
        casualty.ambientTemperatureCelsius,
        casualty.trappedDurationHours,
        casualty.clothingInsulationClo,
        casualty.isWetOrSubmerged
    );
    estimate.estimatedCoreBodyTempCelsius = coreTemp;

    if (coreTemp < 35.0) {
        // Hypothermia lethality multiplier
        double tempDeficit = 35.0 - coreTemp;
        double hypoMultiplier = std::exp(-0.35 * tempDeficit * tempDeficit);
        estimate.overallSurvivalProbability *= hypoMultiplier;

        if (estimate.primaryLethalityThreat == "None" || estimate.primaryLethalityThreat == "Hypothermia") {
            estimate.primaryLethalityThreat = "Hypothermia";
            // Estimate hours remaining until core reaches 26C (fatal cardiac arrest)
            double rateOfCoolingPerHour = (37.0 - coreTemp) / std::max(0.5, casualty.trappedDurationHours);
            if (rateOfCoolingPerHour > 0.1) {
                double remainingDegrees = coreTemp - 26.0;
                estimate.remainingViableRescueHours = std::min(estimate.remainingViableRescueHours, remainingDegrees / rateOfCoolingPerHour);
            }
        }
    }

    // Age vulnerability adjustments: infants and elderly (>65) have reduced physiological reserve
    if (casualty.ageYears > 65 || casualty.ageYears < 5) {
        estimate.overallSurvivalProbability *= 0.80;
    }

    estimate.overallSurvivalProbability = std::clamp(estimate.overallSurvivalProbability, 0.0, 1.0);

    if (estimate.overallSurvivalProbability > 0.10 && estimate.remainingViableRescueHours < 6.0) {
        estimate.isImmediateTriagePriority = true;
    }

    return estimate;
}

} // namespace georescux::analytics
