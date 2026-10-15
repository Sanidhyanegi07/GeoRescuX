#include "PlumeDispersionModel.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_M = 6371008.8;

void forwardGeodesic(
    double lat1, double lon1,
    double distanceMeters, double bearingDeg,
    double& outLat, double& outLon
) noexcept {
    double d = distanceMeters;
    double r = EARTH_RADIUS_M;
    double theta = bearingDeg * DEG2RAD;

    double p1 = lat1 * DEG2RAD;
    double l1 = lon1 * DEG2RAD;

    double sinP1 = std::sin(p1);
    double cosP1 = std::cos(p1);
    double sinDR = std::sin(d / r);
    double cosDR = std::cos(d / r);

    double p2 = std::asin(sinP1 * cosDR + cosP1 * sinDR * std::cos(theta));
    double l2 = l1 + std::atan2(
        std::sin(theta) * sinDR * cosP1,
        cosDR - sinP1 * std::sin(p2)
    );

    outLat = p2 * RAD2DEG;
    outLon = l2 * RAD2DEG;
}
} // namespace

void PlumeDispersionModel::computeDispersionCoefficients(
    double xMeters,
    StabilityClass stability,
    double& outSigmaY,
    double& outSigmaZ
) noexcept {
    double x = std::max(1.0, xMeters);

    // Briggs (1973) Rural Formulations:
    // sigma_y = c * x * (1 + 0.0001 * x)^(-0.5)
    double cy = 0.08;
    switch (stability) {
        case StabilityClass::A_VERY_UNSTABLE: cy = 0.22; break;
        case StabilityClass::B_MODERATELY_UNSTABLE: cy = 0.16; break;
        case StabilityClass::C_SLIGHTLY_UNSTABLE: cy = 0.11; break;
        case StabilityClass::D_NEUTRAL: cy = 0.08; break;
        case StabilityClass::E_SLIGHTLY_STABLE: cy = 0.06; break;
        case StabilityClass::F_MODERATELY_STABLE: cy = 0.04; break;
    }
    outSigmaY = cy * x * std::pow(1.0 + 0.0001 * x, -0.5);

    // sigma_z formulations
    switch (stability) {
        case StabilityClass::A_VERY_UNSTABLE:
            outSigmaZ = 0.20 * x;
            break;
        case StabilityClass::B_MODERATELY_UNSTABLE:
            outSigmaZ = 0.12 * x;
            break;
        case StabilityClass::C_SLIGHTLY_UNSTABLE:
            outSigmaZ = 0.08 * x * std::pow(1.0 + 0.0002 * x, -0.5);
            break;
        case StabilityClass::D_NEUTRAL:
            outSigmaZ = 0.06 * x * std::pow(1.0 + 0.0015 * x, -0.5);
            break;
        case StabilityClass::E_SLIGHTLY_STABLE:
            outSigmaZ = 0.03 * x * std::pow(1.0 + 0.0003 * x, -1.0);
            break;
        case StabilityClass::F_MODERATELY_STABLE:
        default:
            outSigmaZ = 0.016 * x * std::pow(1.0 + 0.0003 * x, -1.0);
            break;
    }
}

double PlumeDispersionModel::calculateBriggsPlumeRise(
    const ChemicalReleaseSource& source,
    double windSpeedMps,
    StabilityClass stability
) noexcept {
    double u = std::max(1.0, windSpeedMps);
    double vs = source.stackGasVelocityMps;
    double d = source.stackDiameterMeters;
    double r = d * 0.5;

    // Buoyancy flux: Fb = g * vs * r^2 * (Ts - Ta) / Ts
    double ts = std::max(200.0, source.stackGasTempKelvin);
    double ta = std::max(200.0, source.ambientTempKelvin);
    constexpr double g = 9.80665;

    if (ts <= ta) {
        // Momentum-dominated plume rise
        return (3.0 * vs * d) / u;
    }

    double fb = g * vs * r * r * ((ts - ta) / ts);

    // Briggs final plume rise for neutral/unstable atmospheres (A, B, C, D)
    if (stability <= StabilityClass::D_NEUTRAL) {
        if (fb < 55.0) {
            return 21.42 * std::pow(fb, 0.75) / u;
        } else {
            return 38.71 * std::pow(fb, 0.60) / u;
        }
    } else {
        // Stable atmosphere (E, F)
        // Stability parameter s = (g / Ta) * (dTheta / dz) ~ 0.00035 for E, 0.00075 for F
        double s = (stability == StabilityClass::E_SLIGHTLY_STABLE) ? 0.00035 : 0.00075;
        return 2.6 * std::pow(fb / (u * s), 1.0 / 3.0);
    }
}

double PlumeDispersionModel::calculateConcentrationMgM3(
    double xDownwindMeters,
    double yCrosswindMeters,
    double windSpeedMps,
    StabilityClass stability,
    const ChemicalReleaseSource& source
) noexcept {
    if (xDownwindMeters <= 1.0) return 0.0;

    double u = std::max(0.5, windSpeedMps);
    double qGramsPerSec = source.emissionRateGramsPerSec;
    double deltaH = calculateBriggsPlumeRise(source, u, stability);
    double hEff = source.releaseHeightMeters + deltaH;

    double sigmaY, sigmaZ;
    computeDispersionCoefficients(xDownwindMeters, stability, sigmaY, sigmaZ);

    if (sigmaY <= 0.0 || sigmaZ <= 0.0) return 0.0;

    // Gaussian Plume ground-level equation:
    // C = (Q / (pi * u * sigma_y * sigma_z)) * exp(-y^2 / (2 * sigma_y^2)) * exp(-H_eff^2 / (2 * sigma_z^2))
    double denom = PI * u * sigmaY * sigmaZ;
    double peakCenterline = qGramsPerSec / denom;

    double crosswindDecay = std::exp(-(yCrosswindMeters * yCrosswindMeters) / (2.0 * sigmaY * sigmaY));
    double verticalDecay = std::exp(-(hEff * hEff) / (2.0 * sigmaZ * sigmaZ));

    // Convert g/m3 to mg/m3 (multiply by 1000)
    return peakCenterline * crosswindDecay * verticalDecay * 1000.0;
}

std::vector<PlumeContourPoint> PlumeDispersionModel::generateExclusionBoundary(
    const ChemicalReleaseSource& source,
    double windDirectionDeg,
    double windSpeedMps,
    StabilityClass stability,
    double idlhThresholdMgM3,
    double maxSearchDistanceMeters
) {
    std::vector<PlumeContourPoint> boundaryPoints;
    double u = std::max(0.5, windSpeedMps);
    double q = source.emissionRateGramsPerSec;
    double deltaH = calculateBriggsPlumeRise(source, u, stability);
    double hEff = source.releaseHeightMeters + deltaH;

    double stepM = 25.0;
    std::vector<std::pair<double, double>> rightSide;
    std::vector<std::pair<double, double>> leftSide;

    for (double x = 25.0; x <= maxSearchDistanceMeters; x += stepM) {
        double sigmaY, sigmaZ;
        computeDispersionCoefficients(x, stability, sigmaY, sigmaZ);

        double denom = PI * u * sigmaY * sigmaZ;
        double peakCenterlineMg = (q / denom) * std::exp(-(hEff * hEff) / (2.0 * sigmaZ * sigmaZ)) * 1000.0;

        if (peakCenterlineMg <= idlhThresholdMgM3) {
            if (!rightSide.empty()) {
                // Plume concentration has dropped below threshold along centerline
                break;
            }
            continue;
        }

        // Solve for yMax where C(x, y) = idlhThresholdMgM3
        // idlh = peakCenterline * exp(-y^2 / (2 * sigma_y^2))
        // y^2 = 2 * sigma_y^2 * ln(peakCenterline / idlh)
        double ratio = peakCenterlineMg / idlhThresholdMgM3;
        double yMax = sigmaY * std::sqrt(2.0 * std::log(ratio));

        rightSide.push_back({x, yMax});
        leftSide.push_back({x, -yMax});
    }

    if (rightSide.empty()) {
        return boundaryPoints;
    }

    // Assemble closed contour loop in clockwise order
    auto projectPoint = [&](double xDown, double yCross) -> PlumeContourPoint {
        double dist = std::sqrt(xDown * xDown + yCross * yCross);
        double localAngleRad = std::atan2(yCross, xDown);
        double absoluteBearing = std::fmod(windDirectionDeg + localAngleRad * RAD2DEG + 360.0, 360.0);

        double ptLat, ptLon;
        forwardGeodesic(source.latitude, source.longitude, dist, absoluteBearing, ptLat, ptLon);

        double conc = calculateConcentrationMgM3(xDown, yCross, windSpeedMps, stability, source);
        return {ptLat, ptLon, conc, xDown, yCross};
    };

    // Forward downwind along right side
    for (const auto& pt : rightSide) {
        boundaryPoints.push_back(projectPoint(pt.first, pt.second));
    }

    // Reverse backwards along left side to close the loop
    for (auto it = leftSide.rbegin(); it != leftSide.rend(); ++it) {
        boundaryPoints.push_back(projectPoint(it->first, it->second));
    }

    return boundaryPoints;
}

} // namespace georescux::backend::spatial
