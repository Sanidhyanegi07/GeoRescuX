#include "MedicalTriageDecisionTree.h"
#include <algorithm>
#include <cmath>

namespace georescux::triage {

double MedicalTriageDecisionTree::calculateRevisedTraumaScore(
    int gcs,
    int systolicBp,
    int respiratoryRate
) noexcept {
    // Champion (1989) Coded Values:
    int gcsCoded = 4;
    if (gcs >= 13) gcsCoded = 4;
    else if (gcs >= 9) gcsCoded = 3;
    else if (gcs >= 6) gcsCoded = 2;
    else if (gcs >= 4) gcsCoded = 1;
    else gcsCoded = 0;

    int sbpCoded = 4;
    if (systolicBp > 89) sbpCoded = 4;
    else if (systolicBp >= 76) sbpCoded = 3;
    else if (systolicBp >= 50) sbpCoded = 2;
    else if (systolicBp >= 1) sbpCoded = 1;
    else sbpCoded = 0;

    int rrCoded = 4;
    if (respiratoryRate >= 10 && respiratoryRate <= 29) rrCoded = 4;
    else if (respiratoryRate > 29) rrCoded = 3;
    else if (respiratoryRate >= 6) rrCoded = 2;
    else if (respiratoryRate >= 1) rrCoded = 1;
    else rrCoded = 0;

    // RTS = 0.9368 * GCS_c + 0.7326 * SBP_c + 0.2908 * RR_c
    return 0.9368 * gcsCoded + 0.7326 * sbpCoded + 0.2908 * rrCoded;
}

TriageClinicalResult MedicalTriageDecisionTree::evaluateAdultStart(
    const ClinicalAssessmentVitals& v
) noexcept {
    TriageClinicalResult r{};
    r.requiresImmediateAirEvacuation = false;

    // TCCC Massive Hemorrhage Check
    if (v.hasUncontrolledExsanguinatingHemorrhage) {
        r.immediateLifeSavingInterventions.push_back("Apply arterial limb tourniquet / wound packing with hemostatic gauze");
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Immediate (Exsanguinating Hemorrhage)";
        return r;
    }

    // Tension Pneumothorax Check
    if (v.hasSuspectedTensionPneumothorax) {
        r.immediateLifeSavingInterventions.push_back("Perform emergency needle thoracostomy (2nd intercostal space)");
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Immediate (Tension Pneumothorax)";
        r.requiresImmediateAirEvacuation = true;
        return r;
    }

    // Step 1: Walking Wounded
    if (v.canWalkSpontaneously) {
        r.tag = TriageTagColor::GREEN_MINOR;
        r.tagDescription = "Minor (Walking Wounded)";
        return r;
    }

    // Step 2: Respiration
    if (!v.isBreathing) {
        if (v.breathingResumedAfterAirwayReposition) {
            r.immediateLifeSavingInterventions.push_back("Insert nasopharyngeal airway (NPA) / maintain recovery position");
            r.tag = TriageTagColor::RED_IMMEDIATE;
            r.tagDescription = "Immediate (Post-Airway Reposition)";
            return r;
        } else {
            r.tag = TriageTagColor::BLACK_EXPECTANT;
            r.tagDescription = "Expectant / Deceased (Apneic)";
            return r;
        }
    }

    if (v.respiratoryRateBpm > 30 || v.respiratoryRateBpm < 10) {
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Immediate (Respiratory Distress)";
        return r;
    }

    // Step 3: Perfusion
    if (!v.hasPalpableRadialPulse || v.capillaryRefillTimeSec > 2.0) {
        r.immediateLifeSavingInterventions.push_back("Control hemorrhage / establish IV/IO access / treat for shock");
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Immediate (Compromised Perfusion / Shock)";
        return r;
    }

    // Step 4: Mental Status
    if (!v.followsSimpleCommands || v.mentalStatus > AvpuMentalStatus::VERBAL_RESPONSE) {
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Immediate (Altered Mental Status)";
        return r;
    }

    // All normal -> Yellow
    r.tag = TriageTagColor::YELLOW_DELAYED;
    r.tagDescription = "Delayed (Serious, non-life-threatening)";
    return r;
}

TriageClinicalResult MedicalTriageDecisionTree::evaluatePediatricJumpStart(
    const ClinicalAssessmentVitals& v
) noexcept {
    TriageClinicalResult r{};
    r.requiresImmediateAirEvacuation = false;

    // Step 1: Ambulatory
    if (v.canWalkSpontaneously) {
        r.tag = TriageTagColor::GREEN_MINOR;
        r.tagDescription = "Pediatric Minor (Ambulatory)";
        return r;
    }

    // Step 2: Breathing
    if (!v.isBreathing) {
        if (v.breathingResumedAfterAirwayReposition) {
            r.tag = TriageTagColor::RED_IMMEDIATE;
            r.tagDescription = "Pediatric Immediate (Respiration Resumed)";
            return r;
        }

        // Check peripheral pulse in apneic child
        if (!v.hasPalpableRadialPulse) {
            r.tag = TriageTagColor::BLACK_EXPECTANT;
            r.tagDescription = "Pediatric Expectant / Deceased (Apneic & Pulseless)";
            return r;
        }

        // 5 Rescue Breaths trial
        if (v.breathingResumedAfterRescueBreaths) {
            r.immediateLifeSavingInterventions.push_back("Administer 5 rescue breaths (bag-valve-mask) / maintain patent airway");
            r.tag = TriageTagColor::RED_IMMEDIATE;
            r.tagDescription = "Pediatric Immediate (Resumed Post-Rescue Breaths)";
            return r;
        } else {
            r.tag = TriageTagColor::BLACK_EXPECTANT;
            r.tagDescription = "Pediatric Expectant (Apneic despite Rescue Breaths)";
            return r;
        }
    }

    // JumpSTART respiratory rate criteria: < 15 or > 45 bpm is RED
    if (v.respiratoryRateBpm < 15 || v.respiratoryRateBpm > 45) {
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Pediatric Immediate (Abnormal Respiratory Rate)";
        return r;
    }

    // Step 3: Perfusion
    if (!v.hasPalpableRadialPulse) {
        r.immediateLifeSavingInterventions.push_back("Establish pediatric vascular access / intraosseous (IO)");
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Pediatric Immediate (Pulseless / Severe Shock)";
        return r;
    }

    // Step 4: Mental Status (AVPU)
    if (v.mentalStatus == AvpuMentalStatus::PAIN_RESPONSE || v.mentalStatus == AvpuMentalStatus::UNRESPONSIVE) {
        r.tag = TriageTagColor::RED_IMMEDIATE;
        r.tagDescription = "Pediatric Immediate (Inappropriate Neurological Response)";
        return r;
    }

    r.tag = TriageTagColor::YELLOW_DELAYED;
    r.tagDescription = "Pediatric Delayed (Stable Vitals)";
    return r;
}

TriageClinicalResult MedicalTriageDecisionTree::evaluateCasualty(
    const ClinicalAssessmentVitals& vitals
) noexcept {
    TriageClinicalResult result = (vitals.ageYears <= 8) ?
        evaluatePediatricJumpStart(vitals) :
        evaluateAdultStart(vitals);

    // Calculate Revised Trauma Score
    result.revisedTraumaScore = calculateRevisedTraumaScore(
        vitals.glasgowComaScale,
        vitals.systolicBloodPressureMmHg,
        vitals.respiratoryRateBpm
    );

    // Champion mortality logistic model: logit = -3.57 + 0.45 * (7.84 - RTS)
    double rtsDeficit = 7.8408 - result.revisedTraumaScore;
    double logit = -3.57 + 0.72 * rtsDeficit;
    result.predictedMortalityProbability = 1.0 / (1.0 + std::exp(-logit));

    if (result.tag == TriageTagColor::RED_IMMEDIATE && result.revisedTraumaScore < 5.0) {
        result.requiresImmediateAirEvacuation = true;
    }

    return result;
}

} // namespace georescux::triage
