#ifndef GEORESCUX_ROUTING_TURN_PENALTY_ENGINE_H
#define GEORESCUX_ROUTING_TURN_PENALTY_ENGINE_H

#include "../spatial/GeodesicMath.h"
#include <cmath>

namespace georescux::routing {

enum class TurnType {
    STRAIGHT,
    SLIGHT_TURN,
    MODERATE_TURN,
    SHARP_TURN,
    HAIRPIN_SWITCHBACK,
    UTURN
};

/**
 * High-precision turn angle and switchback penalty calculation engine.
 * Specifically designed for Himalayan mountain roads where hairpin bends
 * and U-turns drastically slow down evacuation convoys.
 */
class TurnPenaltyEngine {
public:
    static constexpr double HAIRPIN_PENALTY_METERS = 300.0;
    static constexpr double UTURN_PENALTY_METERS = 800.0;
    static constexpr double SHARP_TURN_PENALTY_METERS = 120.0;

    /**
     * Calculates the turn angle between two consecutive road segments:
     * Segment 1: (p1 -> p2)
     * Segment 2: (p2 -> p3)
     * Returns deviation angle in degrees [-180, +180]. (0 = straight, +90 = right, -90 = left).
     */
    static double calculateTurnAngleDegrees(
        double p1Lat, double p1Lon,
        double p2Lat, double p2Lon,
        double p3Lat, double p3Lon
    ) noexcept;

    /**
     * Classifies the turn type based on deviation angle.
     */
    static TurnType classifyTurn(double angleDegrees) noexcept;

    /**
     * Returns the impedance penalty in equivalent distance meters for the turn.
     */
    static double getTurnPenaltyMeters(double angleDegrees, bool isHeavyRescueVehicle = false) noexcept;
};

} // namespace georescux::routing

#endif // GEORESCUX_ROUTING_TURN_PENALTY_ENGINE_H
