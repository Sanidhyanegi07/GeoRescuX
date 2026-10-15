#include "SeismicWaveAttenuationModel.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double EARTH_RADIUS_KM = 6371.0088;

double haversineDistanceKm(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_KM * c;
}

// Standard normal cumulative distribution function approximation (Abramowitz & Stegun)
double normalCdf(double x) noexcept {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}
} // namespace

double SeismicWaveAttenuationModel::ruptureDistanceKm(
    double epiLat, double epiLon, double focalDepthKm,
    double targetLat, double targetLon
) noexcept {
    double epiDist = haversineDistanceKm(epiLat, epiLon, targetLat, targetLon);
    return std::sqrt(epiDist * epiDist + focalDepthKm * focalDepthKm);
}

double SeismicWaveAttenuationModel::pgaToModifiedMercalli(double pgaG) noexcept {
    if (pgaG <= 1e-4) return 1.0;

    // Wald et al. (1999) USGS ShakeMap formulation:
    // Converted to cm/s2 (gal): 1g ~ 980 cm/s2
    double pgaGal = pgaG * 980.665;

    double mmi;
    if (pgaGal <= 35.0) { // low to moderate motion
        mmi = 2.20 * std::log10(pgaGal) + 1.00;
    } else { // strong to catastrophic motion
        mmi = 3.66 * std::log10(pgaGal) - 1.66;
    }

    return std::clamp(mmi, 1.0, 12.0);
}

double SeismicWaveAttenuationModel::calculateStructuralCollapseProbability(
    double mmi,
    StructuralFragilityClass type
) noexcept {
    // Lognormal fragility parameters (mean MMI of complete damage, dispersion beta)
    double meanMmi = 9.0;
    double beta = 0.40;

    switch (type) {
        case StructuralFragilityClass::UNREINFORCED_STONE_MASONRY:
            // High vulnerability: traditional stone-in-mud collapses at MMI VII - VIII
            meanMmi = 7.8;
            beta = 0.35;
            break;
        case StructuralFragilityClass::REINFORCED_CONCRETE_FRAME:
            // Moderate vulnerability: collapses at MMI IX - X
            meanMmi = 9.2;
            beta = 0.42;
            break;
        case StructuralFragilityClass::TIMBER_DHAJJI_DEWARI:
            // Ductile Himalayan vernacular: excellent earthquake resilience
            meanMmi = 10.5;
            beta = 0.50;
            break;
        case StructuralFragilityClass::ENGINEERED_STEEL_STRUCTURE:
            meanMmi = 11.2;
            beta = 0.45;
            break;
    }

    if (mmi <= 4.0) return 0.0;
    double z = (std::log(mmi) - std::log(meanMmi)) / beta;
    return std::clamp(normalCdf(z), 0.0, 1.0);
}

GroundMotionParameters SeismicWaveAttenuationModel::evaluateGroundMotion(
    const SeismicHypocenter& hypo,
    double targetLat,
    double targetLon,
    double vs30Mps,
    StructuralFragilityClass buildingType
) noexcept {
    GroundMotionParameters result{};

    double rRup = ruptureDistanceKm(hypo.epicenterLat, hypo.epicenterLon, hypo.focalDepthKm, targetLat, targetLon);
    double mw = hypo.momentMagnitudeMw;

    // Boore & Atkinson (2008) NGA-West GMPE formulation for active tectonic mountain regions:
    // ln(PGA) = c1 + c2*(Mw - 6.5) + c3*(Mw - 6.5)^2 - c4*ln(sqrt(Rrup^2 + h^2)) + siteAmp
    constexpr double c1 = -0.55;
    constexpr double c2 = 0.45;
    constexpr double c3 = -0.10;
    constexpr double c4 = 1.15;
    constexpr double hFictitious = 7.5; // km

    double magDiff = mw - 6.5;
    double magTerm = c1 + c2 * magDiff + c3 * (magDiff * magDiff);
    double distTerm = -c4 * std::log(std::sqrt(rRup * rRup + hFictitious * hFictitious));

    // Soil amplification relative to reference rock (Vs30 = 760 m/s)
    double effectiveVs30 = std::clamp(vs30Mps, 150.0, 1500.0);
    double siteAmp = -0.36 * std::log(effectiveVs30 / 760.0);

    double lnPga = magTerm + distTerm + siteAmp;
    double pga = std::exp(lnPga);
    result.peakGroundAccelerationG = std::max(1e-5, pga);

    // Peak Ground Velocity (PGV) in cm/s from empirical scaling
    result.peakGroundVelocityCmSec = 115.0 * std::pow(result.peakGroundAccelerationG, 0.88);

    // Spectral Acceleration Sa(1.0s)
    result.spectralAcceleration1s = result.peakGroundAccelerationG * 0.65;

    // Modified Mercalli Intensity
    result.modifiedMercalliIntensity = pgaToModifiedMercalli(result.peakGroundAccelerationG);

    // Structural Collapse Probability
    result.buildingCollapseProbability = calculateStructuralCollapseProbability(
        result.modifiedMercalliIntensity, buildingType
    );

    // Earthquake Early Warning (EEW) lead time:
    // P-wave travels ~ 6.0 km/s, damaging S-wave travels ~ 3.5 km/s
    double pWaveTime = rRup / 6.0;
    double sWaveTime = rRup / 3.5;
    result.estimatedPWaveLeadTimeSec = std::max(0.0, sWaveTime - pWaveTime);

    return result;
}

} // namespace georescux::backend::spatial
