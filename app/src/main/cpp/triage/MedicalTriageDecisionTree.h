#ifndef GEORESCUX_MEDICAL_TRIAGE_DECISION_TREE_H
#define GEORESCUX_MEDICAL_TRIAGE_DECISION_TREE_H

#include <string>
#include <vector>
#include <cstdint>

namespace georescux::triage {

enum class TriageTagColor : uint8_t {
    BLACK_EXPECTANT = 0, // Deceased or non-survivable
    RED_IMMEDIATE = 1,   // Life-threatening, immediate surgical intervention
    YELLOW_DELAYED = 2,  // Serious, non-immediately life-threatening
    GREEN_MINOR = 3      // Walking wounded
};

enum class AvpuMentalStatus : uint8_t {
    ALERT = 0,
    VERBAL_RESPONSE = 1,
    PAIN_RESPONSE = 2,
    UNRESPONSIVE = 3
};

struct ClinicalAssessmentVitals {
    int ageYears;
    bool canWalkSpontaneously;
    bool isBreathing;
    bool breathingResumedAfterAirwayReposition;
    bool breathingResumedAfterRescueBreaths; // JumpSTART pediatric 5 breaths
    int respiratoryRateBpm;
    bool hasPalpableRadialPulse;
    double capillaryRefillTimeSec;
    AvpuMentalStatus mentalStatus;
    bool followsSimpleCommands;
    bool hasUncontrolledExsanguinatingHemorrhage;
    bool hasSuspectedTensionPneumothorax;
    int systolicBloodPressureMmHg;
    int heartRateBpm;
    int glasgowComaScale; // 3 to 15
};

struct TriageClinicalResult {
    TriageTagColor tag;
    std::string tagDescription;
    double revisedTraumaScore;
    double predictedMortalityProbability;
    std::vector<std::string> immediateLifeSavingInterventions;
    bool requiresImmediateAirEvacuation;
};

/**
 * Adult START and Pediatric JumpSTART Mass Casualty Clinical Decision Tree.
 * Implements MARCH protocol (Massive hemorrhage, Airway, Respiration, Circulation, Hypothermia)
 * and Revised Trauma Score (RTS) calculations for field medical disaster triage.
 */
class MedicalTriageDecisionTree {
public:
    MedicalTriageDecisionTree() = default;

    /**
     * Evaluates casualty vitals and assigns definitive triage category.
     * Automatically differentiates pediatric victims (<= 8 years old) to apply JumpSTART.
     */
    [[nodiscard]] static TriageClinicalResult evaluateCasualty(
        const ClinicalAssessmentVitals& vitals
    ) noexcept;

    /**
     * Calculates Champion's Revised Trauma Score (RTS) for physiological triage.
     */
    [[nodiscard]] static double calculateRevisedTraumaScore(
        int gcs,
        int systolicBp,
        int respiratoryRate
    ) noexcept;

private:
    static TriageClinicalResult evaluateAdultStart(const ClinicalAssessmentVitals& v) noexcept;
    static TriageClinicalResult evaluatePediatricJumpStart(const ClinicalAssessmentVitals& v) noexcept;
};

} // namespace georescux::triage

#endif // GEORESCUX_MEDICAL_TRIAGE_DECISION_TREE_H
