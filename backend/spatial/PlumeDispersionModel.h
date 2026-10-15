#ifndef GEORESCUX_BACKEND_PLUME_DISPERSION_MODEL_H
#define GEORESCUX_BACKEND_PLUME_DISPERSION_MODEL_H

#include <vector>
#include <string>
#include <cmath>

namespace georescux::backend::spatial {

enum class StabilityClass {
    A_VERY_UNSTABLE = 1,
    B_MODERATELY_UNSTABLE = 2,
    C_SLIGHTLY_UNSTABLE = 3,
    D_NEUTRAL = 4,        // Overcast skies or high wind
    E_SLIGHTLY_STABLE = 5,// Nighttime clear sky
    F_MODERATELY_STABLE = 6 // Night with calm wind (valley inversion)
};

struct ChemicalReleaseSource {
    double latitude;
    double longitude;
    double emissionRateGramsPerSec; // Q
    double releaseHeightMeters;      // physical release height
    double stackGasVelocityMps;
    double stackGasTempKelvin;
    double ambientTempKelvin;
    double stackDiameterMeters;
};

struct PlumeContourPoint {
    double latitude;
    double longitude;
    double concentrationPpm;
    double downwindDistanceMeters;
    double crosswindDistanceMeters;
};

/**
 * Industrial Chemical, Wildfire Smoke, and Toxic Gas Plume Dispersion Engine.
 * Evaluates Pasquill-Gifford Gaussian plume equations, Briggs buoyant plume rise,
 * and generates emergency evacuation exclusion perimeters around hazardous release sites.
 */
class PlumeDispersionModel {
public:
    PlumeDispersionModel() = default;

    /**
     * Calculates ground-level concentration at downwind distance x and crosswind distance y.
     * @param xDownwindMeters Distance along wind vector (m)
     * @param yCrosswindMeters Distance perpendicular to wind vector (m)
     * @param windSpeedMps Wind speed at 10m height
     * @param stability Atmospheric stability class
     * @param source Release source characteristics
     */
    [[nodiscard]] static double calculateConcentrationMgM3(
        double xDownwindMeters,
        double yCrosswindMeters,
        double windSpeedMps,
        StabilityClass stability,
        const ChemicalReleaseSource& source
    ) noexcept;

    /**
     * Estimates Briggs plume rise (buoyancy and momentum flux).
     */
    [[nodiscard]] static double calculateBriggsPlumeRise(
        const ChemicalReleaseSource& source,
        double windSpeedMps,
        StabilityClass stability
    ) noexcept;

    /**
     * Generates exclusion perimeter polygon (lat/lon) where concentration exceeds threshold.
     */
    [[nodiscard]] static std::vector<PlumeContourPoint> generateExclusionBoundary(
        const ChemicalReleaseSource& source,
        double windDirectionDeg, // direction wind is BLOWING TOWARD (0=North, 90=East)
        double windSpeedMps,
        StabilityClass stability,
        double idlhThresholdMgM3 = 5.0,
        double maxSearchDistanceMeters = 5000.0
    );

private:
    static void computeDispersionCoefficients(
        double xMeters,
        StabilityClass stability,
        double& outSigmaY,
        double& outSigmaZ
    ) noexcept;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_PLUME_DISPERSION_MODEL_H
