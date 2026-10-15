#ifndef GEORESCUX_BACKEND_WILDFIRE_ROTHERMEL_MODEL_H
#define GEORESCUX_BACKEND_WILDFIRE_ROTHERMEL_MODEL_H

#include <cmath>
#include <string>

namespace georescux::backend::spatial {

enum class FuelModelType {
    SHORT_GRASS = 1,
    TIMBER_LITTER = 2,
    CHAPARRAL_SHRUB = 3,
    HEAVY_LOGGING_SLASH = 4,
    PINE_CONIFER_FOREST = 5
};

struct FuelParameters {
    double fuelBedDepthMeters;
    double fuelLoadingKgPerM2;
    double surfaceAreaToVolumeRatio; // 1/m
    double moistureOfExtinction;     // fraction (0.12 - 0.30)
    double heatContentJoulesPerKg;   // ~18,600,000 J/kg
};

/**
 * Standard Rothermel (1972) Surface Fire Rate-of-Spread (ROS) Mathematical Model.
 * Calculates mathematical fire propagation speed, flame length, and heat flux
 * through mountainous pine/deodar forests of Uttarakhand and Himachal Pradesh.
 */
class WildfireRothermelModel {
public:
    static FuelParameters getFuelParams(FuelModelType type) noexcept;

    /**
     * Calculates steady-state rate of spread in meters per minute.
     * @param fuelType Forest fuel categorization
     * @param moistureFraction 1-hour dead fuel moisture (0.03 - 0.25)
     * @param midflameWindSpeedKmh Wind speed at mid-flame height in km/h
     * @param slopeTan Slope tangent (rise / run)
     */
    static double calculateRateOfSpreadMetersPerMin(
        FuelModelType fuelType,
        double moistureFraction,
        double midflameWindSpeedKmh,
        double slopeTan
    ) noexcept;

    /**
     * Estimates Byram's Fireline Intensity (kW/m).
     */
    static double calculateFirelineIntensityKwPerMeter(
        double rateOfSpreadMpm,
        double fuelLoadingKgPerM2,
        double heatContentJoulePerKg = 18600000.0
    ) noexcept;

    /**
     * Estimates flame length in meters from Byram's intensity (Thomas' formula).
     */
    static double estimateFlameLengthMeters(double firelineIntensityKwPerM) noexcept;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_WILDFIRE_ROTHERMEL_MODEL_H
