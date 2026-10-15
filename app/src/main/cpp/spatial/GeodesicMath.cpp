#include "GeodesicMath.h"
#include <algorithm>
#include <cmath>

namespace georescux::spatial {

double GeodesicMath::haversineDistanceMeters(
    double lat1, double lon1,
    double lat2, double lon2
) noexcept {
    const double phi1 = toRadians(lat1);
    const double phi2 = toRadians(lat2);
    const double deltaPhi = toRadians(lat2 - lat1);
    const double deltaLambda = toRadians(lon2 - lon1);

    const double sinDeltaPhiHalf = std::sin(deltaPhi * 0.5);
    const double sinDeltaLambdaHalf = std::sin(deltaLambda * 0.5);

    const double a = (sinDeltaPhiHalf * sinDeltaPhiHalf) +
                     std::cos(phi1) * std::cos(phi2) *
                     (sinDeltaLambdaHalf * sinDeltaLambdaHalf);

    const double clampedA = std::clamp(a, 0.0, 1.0);
    const double c = 2.0 * std::atan2(std::sqrt(clampedA), std::sqrt(1.0 - clampedA));

    return EARTH_RADIUS_METERS * c;
}

double GeodesicMath::vincentyDistanceMeters(
    double lat1, double lon1,
    double lat2, double lon2,
    int maxIterations
) noexcept {
    if (std::abs(lat1 - lat2) < 1e-12 && std::abs(lon1 - lon2) < 1e-12) {
        return 0.0;
    }

    const double phi1 = toRadians(lat1);
    const double phi2 = toRadians(lat2);
    const double L = toRadians(lon2 - lon1);

    const double U1 = std::atan((1.0 - WGS84_F) * std::tan(phi1));
    const double U2 = std::atan((1.0 - WGS84_F) * std::tan(phi2));
    const double sinU1 = std::sin(U1);
    const double cosU1 = std::cos(U1);
    const double sinU2 = std::sin(U2);
    const double cosU2 = std::cos(U2);

    double lambda = L;
    double lambdaPrev = 0.0;
    double cosSqAlpha = 0.0;
    double sinSigma = 0.0;
    double cos2SigmaM = 0.0;
    double cosSigma = 0.0;
    double sigma = 0.0;
    int iteration = 0;

    do {
        const double sinLambda = std::sin(lambda);
        const double cosLambda = std::cos(lambda);

        const double t1 = cosU2 * sinLambda;
        const double t2 = cosU1 * sinU2 - sinU1 * cosU2 * cosLambda;
        sinSigma = std::sqrt(t1 * t1 + t2 * t2);

        if (sinSigma == 0.0) {
            return 0.0; // Coincident points
        }

        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
        sigma = std::atan2(sinSigma, cosSigma);

        const double sinAlpha = (cosU1 * cosU2 * sinLambda) / sinSigma;
        cosSqAlpha = 1.0 - sinAlpha * sinAlpha;

        if (cosSqAlpha != 0.0) {
            cos2SigmaM = cosSigma - (2.0 * sinU1 * sinU2) / cosSqAlpha;
        } else {
            cos2SigmaM = 0.0; // Equatorial line
        }

        const double C = (WGS84_F / 16.0) * cosSqAlpha * (4.0 + WGS84_F * (4.0 - 3.0 * cosSqAlpha));
        lambdaPrev = lambda;
        lambda = L + (1.0 - C) * WGS84_F * sinAlpha *
                 (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM)));

        iteration++;
    } while (std::abs(lambda - lambdaPrev) > 1e-12 && iteration < maxIterations);

    if (iteration >= maxIterations) {
        // Fallback to Haversine if formula failed to converge (e.g. nearly antipodal)
        return haversineDistanceMeters(lat1, lon1, lat2, lon2);
    }

    const double uSq = cosSqAlpha * ((WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_B * WGS84_B));
    const double A = 1.0 + (uSq / 16384.0) * (4096.0 + uSq * (-768.0 + uSq * (320.0 - 175.0 * uSq)));
    const double B = (uSq / 1024.0) * (256.0 + uSq * (-128.0 + uSq * (74.0 - 47.0 * uSq)));
    const double deltaSigma = B * sinSigma * (cos2SigmaM + 0.25 * B * (cosSigma * (-1.0 + 2.0 * cos2SigmaM * cos2SigmaM) -
                              (1.0 / 6.0) * B * cos2SigmaM * (-3.0 + 4.0 * sinSigma * sinSigma) * (-3.0 + 4.0 * cos2SigmaM * cos2SigmaM)));

    return WGS84_B * A * (sigma - deltaSigma);
}

double GeodesicMath::initialBearingDegrees(
    double lat1, double lon1,
    double lat2, double lon2
) noexcept {
    const double phi1 = toRadians(lat1);
    const double phi2 = toRadians(lat2);
    const double deltaLambda = toRadians(lon2 - lon1);

    const double y = std::sin(deltaLambda) * std::cos(phi2);
    const double x = std::cos(phi1) * std::sin(phi2) -
                     std::sin(phi1) * std::cos(phi2) * std::cos(deltaLambda);

    double bearing = toDegrees(std::atan2(y, x));
    return std::fmod(bearing + 360.0, 360.0);
}

std::pair<double, double> GeodesicMath::destinationPoint(
    double lat, double lon,
    double distanceMeters,
    double bearingDegrees
) noexcept {
    const double delta = distanceMeters / EARTH_RADIUS_METERS;
    const double theta = toRadians(bearingDegrees);
    const double phi1 = toRadians(lat);
    const double lambda1 = toRadians(lon);

    const double sinPhi1 = std::sin(phi1);
    const double cosPhi1 = std::cos(phi1);
    const double sinDelta = std::sin(delta);
    const double cosDelta = std::cos(delta);

    const double phi2 = std::asin(sinPhi1 * cosDelta + cosPhi1 * sinDelta * std::cos(theta));
    const double lambda2 = lambda1 + std::atan2(
        std::sin(theta) * sinDelta * cosPhi1,
        cosDelta - sinPhi1 * std::sin(phi2)
    );

    double destLat = toDegrees(phi2);
    double destLon = std::fmod(toDegrees(lambda2) + 540.0, 360.0) - 180.0;

    return {destLat, destLon};
}

double GeodesicMath::crossTrackDistanceMeters(
    double pathStartLat, double pathStartLon,
    double pathEndLat, double pathEndLon,
    double pointLat, double pointLon
) noexcept {
    const double distStartToPoint = haversineDistanceMeters(pathStartLat, pathStartLon, pointLat, pointLon) / EARTH_RADIUS_METERS;
    const double bearingStartToPoint = toRadians(initialBearingDegrees(pathStartLat, pathStartLon, pointLat, pointLon));
    const double bearingPath = toRadians(initialBearingDegrees(pathStartLat, pathStartLon, pathEndLat, pathEndLon));

    const double crossTrackAngular = std::asin(std::sin(distStartToPoint) * std::sin(bearingStartToPoint - bearingPath));
    return crossTrackAngular * EARTH_RADIUS_METERS;
}

} // namespace georescux::spatial
