#ifndef GEORESCUX_CASUALTY_SURVIVAL_PROBABILITY_H
#define GEORESCUX_CASUALTY_SURVIVAL_PROBABILITY_H

#include <vector>
#include <string>
#include <cmath>

namespace georescux::analytics {

enum class EntrapmentEnvironment {
    CONCRETE_RUBBLE_COLLAPSE, // Structural earthquake collapse void
    SNOW_AVALANCHE_BURIAL,    // Snow burial with/without air pocket
    MUDSLIDE_DEBRIS_BURIAL,   // Landslide mud engulfment
    OPEN_TERRAIN_EXPOSURE     // Stranded in cold rain/high altitude wind
};

struct CasualtyProfile {
    int ageYears;
    double trappedDurationHours;
    double ambientTemperatureCelsius;
    bool isWetOrSubmerged;
    bool hasSevereBleeding;
    bool hasCrushSyndromeRisk; // Limb compressed under heavy weight > 4 hrs
    bool hasConfirmedAirPocket;
    double clothingInsulationClo; // 0.5 (light) to 3.0 (heavy arctic gear)
};

struct SurvivalEstimate {
    double overallSurvivalProbability; // 0.0 to 1.0
    double estimatedCoreBodyTempCelsius;
    double remainingViableRescueHours;
    bool isImmediateTriagePriority;
    std::string primaryLethalityThreat; // "Asphyxiation", "Hypothermia", "Exsanguination", "Crush Synergism"
};

/**
 * Mathematical Casualty Survivability and Hypothermia Engine.
 * Predicts human survival probability over time in rubble, avalanches,
 * and high-altitude Himalayan freezing conditions using Hayward core temperature dynamics
 * and multi-factor Bayesian physiological mortality models.
 */
class CasualtySurvivalProbability {
public:
    CasualtySurvivalProbability() = default;

    /**
     * Computes the survival probability and physiological status of a trapped casualty.
     */
    [[nodiscard]] static SurvivalEstimate computeSurvivalProbability(
        const CasualtyProfile& casualty,
        EntrapmentEnvironment env
    ) noexcept;

    /**
     * Estimates core body temperature drop over time using Hayward thermal balance.
     */
    [[nodiscard]] static double estimateCoreTemperatureCelsius(
        double initialCoreTempC,
        double ambientTempC,
        double durationHours,
        double cloInsulation,
        bool isWet
    ) noexcept;

    /**
     * Calculates avalanche burial survival curve (Falk & Brugger Swiss avalanche mortality model).
     */
    [[nodiscard]] static double calculateAvalancheBurialSurvival(
        double burialDurationMinutes,
        bool hasAirPocket
    ) noexcept;
};

} // namespace georescux::analytics

#endif // GEORESCUX_CASUALTY_SURVIVAL_PROBABILITY_H
