#ifndef GEORESCUX_TRIAGE_ENGINE_H
#define GEORESCUX_TRIAGE_ENGINE_H

#include <cstdint>
#include <string>

namespace georescux::triage {

enum class TriageCategory : uint8_t {
    BLACK = 0,   // Expectant / Deceased (lowest transport priority)
    GREEN = 1,   // Minor / Walking wounded
    YELLOW = 2,  // Delayed / Serious but not immediately life-threatening
    RED = 3      // Immediate / Critical life-threatening
};

struct VictimAssessment {
    bool canWalk{false};
    bool breathingSpontaneously{false};
    bool breathingAfterAirwayPositioned{false};
    uint8_t respirationRatePerMinute{0};
    double capillaryRefillSeconds{0.0};
    bool radialPulsePresent{false};
    bool followsSimpleCommands{false};
    bool hasUncontrolledHemorrhage{false};
    bool isTrapped{false};
};

struct TriageResult {
    TriageCategory category;
    uint8_t priorityScore; // 0 - 100 (100 = most urgent)
    std::string primaryReason;
    std::string recommendedIntervention;
};

/**
 * Standardized Mass Casualty Incident (MCI) triage engine.
 * Implements the START (Simple Triage And Rapid Treatment) and
 * SALT triage protocols for disaster responders.
 */
class TriageEngine {
public:
    /**
     * Evaluates victim vitals using the START algorithm.
     */
    static TriageResult evaluateStart(const VictimAssessment& assessment) noexcept;

    /**
     * Calculates a continuous urgency score [0, 100] for sorting rescue dispatch queues.
     */
    static uint8_t computeContinuousUrgencyScore(
        TriageCategory category,
        bool hasHemorrhage,
        bool isTrapped,
        uint8_t respirationRate,
        bool radialPulse
    ) noexcept;

    /**
     * Converts a TriageCategory to its human-readable code.
     */
    static const char* categoryToString(TriageCategory cat) noexcept;
};

} // namespace georescux::triage

#endif // GEORESCUX_TRIAGE_ENGINE_H
