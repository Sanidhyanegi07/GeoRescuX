#include "TriageEngine.h"
#include <algorithm>

namespace georescux::triage {

const char* TriageEngine::categoryToString(TriageCategory cat) noexcept {
    switch (cat) {
        case TriageCategory::RED: return "RED";
        case TriageCategory::YELLOW: return "YELLOW";
        case TriageCategory::GREEN: return "GREEN";
        case TriageCategory::BLACK: return "BLACK";
    }
    return "UNKNOWN";
}

TriageResult TriageEngine::evaluateStart(const VictimAssessment& a) noexcept {
    // Step 1: Minor / Walking Wounded
    if (a.canWalk) {
        return TriageResult{
            TriageCategory::GREEN,
            25,
            "Ambulatory (walking wounded)",
            "Direct to secondary collection point"
        };
    }

    // Step 2: Respiration check
    if (!a.breathingSpontaneously) {
        if (a.breathingAfterAirwayPositioned) {
            return TriageResult{
                TriageCategory::RED,
                95,
                "Apneic restored after positioning airway",
                "Maintain open airway, immediate transport"
            };
        } else {
            return TriageResult{
                TriageCategory::BLACK,
                0,
                "Deceased / non-salvageable in current resource context",
                "Palliative / comfort care only if resources allow"
            };
        }
    }

    // Respiration rate check (<10 or >30 per min is critical)
    if (a.respirationRatePerMinute > 30 || (a.respirationRatePerMinute < 10 && a.respirationRatePerMinute > 0)) {
        return TriageResult{
            TriageCategory::RED,
            90,
            "Tachypnea (>30/min) or severe bradypnea (<10/min)",
            "Oxygen support, immediate evacuation"
        };
    }

    // Step 3: Perfusion check (radial pulse or capillary refill > 2 sec)
    if (!a.radialPulsePresent || a.capillaryRefillSeconds > 2.0) {
        return TriageResult{
            TriageCategory::RED,
            88,
            "Compromised perfusion (absent radial pulse / capillary refill >2s)",
            "Control bleeding, shock management, immediate evacuation"
        };
    }

    // Uncontrolled severe hemorrhage
    if (a.hasUncontrolledHemorrhage) {
        return TriageResult{
            TriageCategory::RED,
            92,
            "Uncontrolled active hemorrhage",
            "Apply tourniquet / pressure dressing immediately"
        };
    }

    // Step 4: Mental status check
    if (!a.followsSimpleCommands) {
        return TriageResult{
            TriageCategory::RED,
            82,
            "Altered mental status (unable to follow simple commands)",
            "Spinal precaution, immediate neurological evacuation"
        };
    }

    // Step 5: Delayed
    uint8_t delayedScore = 55;
    if (a.isTrapped) delayedScore += 15;

    return TriageResult{
        TriageCategory::YELLOW,
        delayedScore,
        "Serious non-life-threatening injury (stable vitals)",
        "Splint fractures, wound dressing, delayed evacuation"
    };
}

uint8_t TriageEngine::computeContinuousUrgencyScore(
    TriageCategory category,
    bool hasHemorrhage,
    bool isTrapped,
    uint8_t respirationRate,
    bool radialPulse
) noexcept {
    int score = 0;
    switch (category) {
        case TriageCategory::RED:
            score = 75;
            break;
        case TriageCategory::YELLOW:
            score = 45;
            break;
        case TriageCategory::GREEN:
            score = 20;
            break;
        case TriageCategory::BLACK:
            return 0;
    }

    if (hasHemorrhage) score += 15;
    if (isTrapped) score += 8;
    if (!radialPulse && category == TriageCategory::RED) score += 7;
    if (respirationRate > 35) score += 5;

    return static_cast<uint8_t>(std::clamp(score, 0, 100));
}

} // namespace georescux::triage
