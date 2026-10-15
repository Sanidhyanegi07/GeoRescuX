#include "LineOfSightElevationEngine.h"
#include <algorithm>
#include <cmath>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_M = 6371008.8;

double haversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_M * c;
}

void intermediatePoint(
    double lat1, double lon1,
    double lat2, double lon2,
    double fraction,
    double& outLat, double& outLon
) noexcept {
    double p1 = lat1 * DEG2RAD;
    double l1 = lon1 * DEG2RAD;
    double p2 = lat2 * DEG2RAD;
    double l2 = lon2 * DEG2RAD;

    double d = 2.0 * std::asin(std::sqrt(
        std::pow(std::sin((p1 - p2) / 2.0), 2.0) +
        std::cos(p1) * std::cos(p2) * std::pow(std::sin((l1 - l2) / 2.0), 2.0)
    ));

    if (d < 1e-9) {
        outLat = lat1;
        outLon = lon1;
        return;
    }

    double a = std::sin((1.0 - fraction) * d) / std::sin(d);
    double b = std::sin(fraction * d) / std::sin(d);

    double x = a * std::cos(p1) * std::cos(l1) + b * std::cos(p2) * std::cos(l2);
    double y = a * std::cos(p1) * std::sin(l1) + b * std::cos(p2) * std::sin(l2);
    double z = a * std::sin(p1) + b * std::sin(p2);

    outLat = std::atan2(z, std::sqrt(x * x + y * y)) * RAD2DEG;
    outLon = std::atan2(y, x) * RAD2DEG;
}
} // namespace

LineOfSightElevationEngine::LineOfSightElevationEngine(const DigitalElevationModel& dem)
    : dem_(dem) {}

double LineOfSightElevationEngine::calculateFresnelRadiusMeters(
    double d1Meters,
    double d2Meters,
    double totalDistanceMeters,
    double frequencyMhz
) noexcept {
    if (totalDistanceMeters <= 0.0 || frequencyMhz <= 0.0) return 0.0;
    // Speed of light c = 3e8 m/s -> wavelength lambda = c / f
    double lambdaMeters = 299.792458 / frequencyMhz;
    double num = lambdaMeters * d1Meters * d2Meters;
    return std::sqrt(std::max(0.0, num / totalDistanceMeters));
}

double LineOfSightElevationEngine::calculateFsplDb(double distanceMeters, double frequencyMhz) noexcept {
    double distKm = std::max(0.001, distanceMeters / 1000.0);
    double fMhz = std::max(1.0, frequencyMhz);
    // Standard FSPL formula: 20*log10(d_km) + 20*log10(f_MHz) + 32.44
    return 20.0 * std::log10(distKm) + 20.0 * std::log10(fMhz) + 32.44;
}

RfPropagationResult LineOfSightElevationEngine::evaluateRadioLink(
    const RfAntennaConfig& tx,
    const RfAntennaConfig& rx,
    double sampleStepMeters
) const {
    RfPropagationResult result{};
    result.hasDirectLineOfSight = true;
    result.hasFresnelClearance = true;
    result.diffractionLossDb = 0.0;

    double totalDist = haversineDistanceMeters(tx.latitude, tx.longitude, rx.latitude, rx.longitude);
    result.totalDistanceMeters = totalDist;

    if (totalDist <= 1.0) {
        result.freeSpacePathLossDb = 0.0;
        result.totalPathLossDb = 0.0;
        result.estimatedRssiDbm = tx.transmitPowerDbm;
        result.isLinkViable = true;
        return result;
    }

    double txGroundElev = dem_.getElevationAt(tx.latitude, tx.longitude);
    double rxGroundElev = dem_.getElevationAt(rx.latitude, rx.longitude);

    double txAntennaAlt = txGroundElev + tx.towerHeightAboveGroundMeters;
    double rxAntennaAlt = rxGroundElev + rx.towerHeightAboveGroundMeters;

    size_t numSamples = std::max(size_t{2}, static_cast<size_t>(std::ceil(totalDist / sampleStepMeters)));
    double worstClearanceFraction = 1.0;
    double maxObstructionHeight = 0.0;
    double worstD1 = totalDist * 0.5;
    double worstD2 = totalDist * 0.5;

    for (size_t i = 1; i < numSamples; ++i) {
        double fraction = static_cast<double>(i) / numSamples;
        double d1 = fraction * totalDist;
        double d2 = totalDist - d1;

        double sampleLat, sampleLon;
        intermediatePoint(tx.latitude, tx.longitude, rx.latitude, rx.longitude, fraction, sampleLat, sampleLon);

        double terrainElev = dem_.getElevationAt(sampleLat, sampleLon);
        double rayHeight = txAntennaAlt + fraction * (rxAntennaAlt - txAntennaAlt);
        double fresnelR = calculateFresnelRadiusMeters(d1, d2, totalDist, tx.frequencyMhz);

        double clearance = rayHeight - terrainElev;
        double clearanceFraction = (fresnelR > 0.01) ? (clearance / fresnelR) : 1.0;

        if (clearance < 0.0) {
            result.hasDirectLineOfSight = false;
            double obstH = terrainElev - rayHeight;
            if (obstH > maxObstructionHeight) {
                maxObstructionHeight = obstH;
                worstD1 = d1;
                worstD2 = d2;
            }
        }
        if (clearanceFraction < 0.60) {
            result.hasFresnelClearance = false;
        }
        if (clearanceFraction < worstClearanceFraction) {
            worstClearanceFraction = clearanceFraction;
        }

        result.profileSamples.push_back({
            sampleLat,
            sampleLon,
            d1,
            terrainElev,
            rayHeight,
            fresnelR,
            clearanceFraction
        });
    }

    result.freeSpacePathLossDb = calculateFsplDb(totalDist, tx.frequencyMhz);

    // Knife-Edge Diffraction Loss (Lee / ITU-R P.526 approximation)
    if (!result.hasDirectLineOfSight) {
        double lambda = 299.792458 / tx.frequencyMhz;
        double nu = maxObstructionHeight * std::sqrt((2.0 / lambda) * (1.0 / worstD1 + 1.0 / worstD2));
        if (nu > -0.7) {
            result.diffractionLossDb = 6.9 + 20.0 * std::log10(std::sqrt((nu - 0.1) * (nu - 0.1) + 1.0) + nu - 0.1);
        }
    } else if (!result.hasFresnelClearance) {
        // Sub-Fresnel clearance attenuation (up to 6 dB loss)
        double deficit = std::max(0.0, 0.60 - worstClearanceFraction);
        result.diffractionLossDb = (deficit / 0.60) * 6.0;
    }

    result.totalPathLossDb = result.freeSpacePathLossDb + result.diffractionLossDb;
    result.estimatedRssiDbm = tx.transmitPowerDbm - result.totalPathLossDb;
    result.isLinkViable = (result.estimatedRssiDbm >= rx.receiverSensitivityDbm);

    return result;
}

} // namespace georescux::spatial
