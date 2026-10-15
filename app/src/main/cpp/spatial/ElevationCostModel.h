#ifndef GEORESCUX_SPATIAL_ELEVATION_COST_MODEL_H
#define GEORESCUX_SPATIAL_ELEVATION_COST_MODEL_H

#include <cmath>

namespace georescux::spatial {

/**
 * Terrain elevation and slope cost model for disaster evacuation in mountainous regions.
 * Uses empirical biomechanical models (Tobler's Hiking Function + Minetti energy expenditure)
 * to compute true evacuation effort and duration across mountain gradients.
 */
class ElevationCostModel {
public:
    static constexpr double BASE_WALKING_SPEED_KMH = 5.0; // Flat ground walking speed (km/h)
    static constexpr double HIGH_ALTITUDE_THRESHOLD_METERS = 2400.0; // Onset of altitude fatigue

    /**
     * Calculates walking velocity in km/h based on Tobler's Hiking Function.
     * slope = (rise / run) = deltaElevation / horizontalDistance
     */
    static inline double toblerSpeedKmh(double slope) noexcept {
        // Tobler formula: V = 6.0 * exp(-3.5 * abs(slope + 0.05))
        const double exponent = -3.5 * std::abs(slope + 0.05);
        return 6.0 * std::exp(exponent);
    }

    /**
     * Calculates the effective impedance (effective distance multiplier)
     * for a path segment based on elevation change.
     */
    static double calculateImpedanceMultiplier(
        double fromElevationMeters,
        double toElevationMeters,
        double horizontalDistanceMeters,
        bool evacueeHasInjuries = false
    ) noexcept;

    /**
     * Estimates travel duration in seconds across an edge considering slope and altitude.
     */
    static double estimateDurationSeconds(
        double fromElevationMeters,
        double toElevationMeters,
        double horizontalDistanceMeters,
        bool evacueeHasInjuries = false
    ) noexcept;

    /**
     * Calculates energy expenditure in Joules per kilogram per meter (Minetti et al.)
     */
    static double metabolicCostJoulesPerKgPerMeter(double slope) noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_SPATIAL_ELEVATION_COST_MODEL_H
