#include "ElevationCostModel.h"
#include <algorithm>

namespace georescux::spatial {

double ElevationCostModel::calculateImpedanceMultiplier(
    double fromElevationMeters,
    double toElevationMeters,
    double horizontalDistanceMeters,
    bool evacueeHasInjuries
) noexcept {
    if (horizontalDistanceMeters <= 0.1) return 1.0;

    const double deltaElevation = toElevationMeters - fromElevationMeters;
    const double slope = deltaElevation / horizontalDistanceMeters;

    double speed = toblerSpeedKmh(slope);
    // Clamp minimum speed to 0.5 km/h for extreme scrambles
    speed = std::clamp(speed, 0.5, 6.5);

    double multiplier = BASE_WALKING_SPEED_KMH / speed;

    // High altitude hypoxia penalty (>2400m)
    const double avgAlt = (fromElevationMeters + toElevationMeters) * 0.5;
    if (avgAlt > HIGH_ALTITUDE_THRESHOLD_METERS) {
        const double excess = (avgAlt - HIGH_ALTITUDE_THRESHOLD_METERS) / 1000.0;
        multiplier *= (1.0 + 0.15 * excess); // +15% per 1000m above 2400m
    }

    // Injured evacuee multiplier (steep slopes are doubly punishing)
    if (evacueeHasInjuries) {
        multiplier *= (1.0 + 2.0 * std::abs(slope));
    }

    return std::max(1.0, multiplier);
}

double ElevationCostModel::estimateDurationSeconds(
    double fromElevationMeters,
    double toElevationMeters,
    double horizontalDistanceMeters,
    bool evacueeHasInjuries
) noexcept {
    if (horizontalDistanceMeters <= 0.0) return 0.0;

    const double deltaElevation = toElevationMeters - fromElevationMeters;
    const double slope = deltaElevation / horizontalDistanceMeters;

    double speedKmh = toblerSpeedKmh(slope);
    speedKmh = std::clamp(speedKmh, 0.5, 6.5);

    double speedMs = (speedKmh * 1000.0) / 3600.0;

    // Hypoxia reduction
    const double avgAlt = (fromElevationMeters + toElevationMeters) * 0.5;
    if (avgAlt > HIGH_ALTITUDE_THRESHOLD_METERS) {
        const double excess = (avgAlt - HIGH_ALTITUDE_THRESHOLD_METERS) / 1000.0;
        speedMs /= (1.0 + 0.15 * excess);
    }

    if (evacueeHasInjuries) {
        speedMs *= 0.55; // Injured walk at 55% speed
    }

    return horizontalDistanceMeters / std::max(0.2, speedMs);
}

double ElevationCostModel::metabolicCostJoulesPerKgPerMeter(double slope) noexcept {
    // Minetti et al. (2002) 5th-order polynomial for energy expenditure on gradient
    const double i = slope;
    const double i2 = i * i;
    const double i3 = i2 * i;
    const double i4 = i3 * i;
    const double i5 = i4 * i;

    const double c = 155.4 * i5 - 30.4 * i4 - 43.3 * i3 + 46.3 * i2 + 19.5 * i + 3.6;
    return std::max(1.5, c);
}

} // namespace georescux::spatial
