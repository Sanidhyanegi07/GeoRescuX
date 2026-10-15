#include "WildfireRothermelModel.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

FuelParameters WildfireRothermelModel::getFuelParams(FuelModelType type) noexcept {
    switch (type) {
        case FuelModelType::SHORT_GRASS: // Anderson Model 1 (Short Grass / Pasture)
            return {
                .fuelBedDepthMeters = 0.30,
                .fuelLoadingKgPerM2 = 0.166,
                .surfaceAreaToVolumeRatio = 11480.0, // 3500 1/ft -> ~11480 1/m
                .moistureOfExtinction = 0.12,
                .heatContentJoulesPerKg = 18600000.0
            };
        case FuelModelType::TIMBER_LITTER: // Anderson Model 8 (Closed Timber Litter / Deodar/Pine)
            return {
                .fuelBedDepthMeters = 0.06,
                .fuelLoadingKgPerM2 = 0.336,
                .surfaceAreaToVolumeRatio = 6160.0, // 1880 1/ft -> ~6160 1/m
                .moistureOfExtinction = 0.30,
                .heatContentJoulesPerKg = 18600000.0
            };
        case FuelModelType::CHAPARRAL_SHRUB: // Anderson Model 4 (High Chaparral / Dense Scrub)
            return {
                .fuelBedDepthMeters = 1.83,
                .fuelLoadingKgPerM2 = 1.123,
                .surfaceAreaToVolumeRatio = 6560.0, // 2000 1/ft
                .moistureOfExtinction = 0.20,
                .heatContentJoulesPerKg = 18600000.0
            };
        case FuelModelType::HEAVY_LOGGING_SLASH: // Anderson Model 10 (Timber with deadfall/litter)
            return {
                .fuelBedDepthMeters = 0.30,
                .fuelLoadingKgPerM2 = 0.675,
                .surfaceAreaToVolumeRatio = 6560.0,
                .moistureOfExtinction = 0.25,
                .heatContentJoulesPerKg = 18600000.0
            };
        case FuelModelType::PINE_CONIFER_FOREST: // Chir Pine Himalayan Forest Litter
        default:
            return {
                .fuelBedDepthMeters = 0.15,
                .fuelLoadingKgPerM2 = 0.450,
                .surfaceAreaToVolumeRatio = 5740.0, // 1750 1/ft
                .moistureOfExtinction = 0.25,
                .heatContentJoulesPerKg = 19500000.0
            };
    }
}

double WildfireRothermelModel::calculateRateOfSpreadMetersPerMin(
    FuelModelType fuelType,
    double moistureFraction,
    double midflameWindSpeedKmh,
    double slopeTan
) noexcept {
    const FuelParameters p = getFuelParams(fuelType);

    // Particle density ~ 513 kg/m3 (oven-dry wood particle density: 32 lb/ft3)
    constexpr double rhoP = 512.6; 
    constexpr double totalMineralContent = 0.055; // St
    constexpr double silicaFreeMineralContent = 0.010; // Se

    // Effective fuel depth and bulk density
    double depth = std::max(0.01, p.fuelBedDepthMeters);
    double w0 = std::max(0.01, p.fuelLoadingKgPerM2);
    double rhoB = w0 / depth; // bulk density kg/m3
    double beta = rhoB / rhoP; // packing ratio

    // Surface area to volume ratio sigma in 1/ft for Rothermel standard equations
    double sigmaFt = p.surfaceAreaToVolumeRatio * 0.3048; // convert 1/m to 1/ft
    double betaOp = 3.348 * std::pow(sigmaFt, -0.8189);
    double betaRatio = beta / betaOp;

    // Optimum reaction velocity A & GammaMax
    double A = 133.0 * std::pow(sigmaFt, -0.7913);
    double gammaPrimeMax = std::pow(sigmaFt, 1.5) / (495.0 + 0.0594 * std::pow(sigmaFt, 1.5));
    double gammaPrime = gammaPrimeMax * std::pow(betaRatio, A) * std::exp(A * (1.0 - betaRatio));

    // Moisture damping coefficient etaM
    double mx = std::max(0.05, p.moistureOfExtinction);
    double mf = std::clamp(moistureFraction, 0.01, mx);
    double rm = mf / mx;
    double etaM = std::max(0.0, 1.0 - 2.59 * rm + 5.11 * std::pow(rm, 2.0) - 3.52 * std::pow(rm, 3.0));

    // Mineral damping coefficient etaS
    double etaS = std::clamp(0.174 * std::pow(silicaFreeMineralContent, -0.19), 0.1, 1.0);

    // Net fuel loading wn
    double wn = w0 * (1.0 - totalMineralContent);

    // Reaction intensity I_R (Joules/m2-min)
    // GammaPrime is 1/min in imperial; here we calculate metric reaction intensity
    double reactionIntensity = gammaPrime * wn * p.heatContentJoulesPerKg * etaM * etaS;

    // Propagating flux ratio xi
    double sigmaM = p.surfaceAreaToVolumeRatio;
    double xi = std::exp((0.792 + 0.681 * std::sqrt(sigmaM)) * (beta + 0.1)) / 
                (192.0 + 0.2595 * sigmaM);

    // Wind factor phiW
    double windMpm = std::max(0.0, (midflameWindSpeedKmh * 1000.0) / 60.0);
    double windFpm = windMpm * 3.28084;
    double C = 7.47 * std::exp(-0.133 * std::pow(sigmaFt, 0.55));
    double B = 0.02526 * std::pow(sigmaFt, 0.54);
    double E = 0.715 * std::exp(-3.59e-4 * sigmaFt);
    double phiW = C * std::pow(windFpm, B) * std::pow(betaRatio, -E);

    // Slope factor phiS
    double s = std::max(0.0, slopeTan);
    double phiS = 5.275 * std::pow(beta, -0.3) * (s * s);

    // Effective heating number epsilon
    double epsilon = std::exp(-138.0 / sigmaFt);

    // Heat of preignition Qig (J/kg)
    // Rothermel: Qig = 250 + 1116 * Mf (BTU/lb) -> 1 BTU/lb = 2326 J/kg
    double qigBtuPerLb = 250.0 + 1116.0 * mf;
    double qigJoulesPerKg = qigBtuPerLb * 2326.0;

    // Denominator: Heat sink rhoB * epsilon * Qig
    double heatSink = rhoB * epsilon * qigJoulesPerKg;
    if (heatSink <= 1e-6) {
        return 0.0;
    }

    // Rate of spread (meters / min)
    double rateOfSpread = (reactionIntensity * xi * (1.0 + phiW + phiS)) / heatSink;
    return std::max(0.0, rateOfSpread);
}

double WildfireRothermelModel::calculateFirelineIntensityKwPerMeter(
    double rateOfSpreadMpm,
    double fuelLoadingKgPerM2,
    double heatContentJoulePerKg
) noexcept {
    // Byram's equation: I_B = H * w_a * R
    // R in m/s = rateOfSpreadMpm / 60.0
    // w_a in kg/m2
    // H in kJ/kg = heatContentJoulePerKg / 1000.0
    // I_B in kW/m
    double rMetersPerSec = std::max(0.0, rateOfSpreadMpm / 60.0);
    double heatKjPerKg = heatContentJoulePerKg / 1000.0;
    return heatKjPerKg * fuelLoadingKgPerM2 * rMetersPerSec;
}

double WildfireRothermelModel::estimateFlameLengthMeters(double firelineIntensityKwPerM) noexcept {
    if (firelineIntensityKwPerM <= 0.0) return 0.0;
    // Byram (1959) / Thomas (1963): Flame length L = 0.0775 * (I_B)^0.46
    return 0.0775 * std::pow(firelineIntensityKwPerM, 0.46);
}

} // namespace georescux::backend::spatial
