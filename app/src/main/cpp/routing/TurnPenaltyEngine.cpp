#include "TurnPenaltyEngine.h"
#include <algorithm>

namespace georescux::routing {

double TurnPenaltyEngine::calculateTurnAngleDegrees(
    double p1Lat, double p1Lon,
    double p2Lat, double p2Lon,
    double p3Lat, double p3Lon
) noexcept {
    const double b1 = spatial::GeodesicMath::initialBearingDegrees(p1Lat, p1Lon, p2Lat, p2Lon);
    const double b2 = spatial::GeodesicMath::initialBearingDegrees(p2Lat, p2Lon, p3Lat, p3Lon);

    double diff = b2 - b1;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    return diff;
}

TurnType TurnPenaltyEngine::classifyTurn(double angleDegrees) noexcept {
    const double absAngle = std::abs(angleDegrees);
    if (absAngle < 15.0) return TurnType::STRAIGHT;
    if (absAngle < 45.0) return TurnType::SLIGHT_TURN;
    if (absAngle < 90.0) return TurnType::MODERATE_TURN;
    if (absAngle < 135.0) return TurnType::SHARP_TURN;
    if (absAngle < 165.0) return TurnType::HAIRPIN_SWITCHBACK;
    return TurnType::UTURN;
}

double TurnPenaltyEngine::getTurnPenaltyMeters(double angleDegrees, bool isHeavyRescueVehicle) noexcept {
    const TurnType type = classifyTurn(angleDegrees);
    double penalty = 0.0;

    switch (type) {
        case TurnType::STRAIGHT:
            penalty = 0.0;
            break;
        case TurnType::SLIGHT_TURN:
            penalty = 15.0;
            break;
        case TurnType::MODERATE_TURN:
            penalty = 50.0;
            break;
        case TurnType::SHARP_TURN:
            penalty = SHARP_TURN_PENALTY_METERS;
            break;
        case TurnType::HAIRPIN_SWITCHBACK:
            penalty = HAIRPIN_PENALTY_METERS;
            break;
        case TurnType::UTURN:
            penalty = UTURN_PENALTY_METERS;
            break;
    }

    if (isHeavyRescueVehicle) {
        // Heavy rescue vehicles / ambulances suffer 2x penalty on hairpins and U-turns
        if (type == TurnType::HAIRPIN_SWITCHBACK || type == TurnType::UTURN) {
            penalty *= 2.2;
        } else {
            penalty *= 1.5;
        }
    }

    return penalty;
}

} // namespace georescux::routing
