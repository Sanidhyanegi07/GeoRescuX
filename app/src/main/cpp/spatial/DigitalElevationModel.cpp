#include "DigitalElevationModel.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace georescux::spatial {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_METERS = 6371008.8;

double haversineDistanceMeters(double lat1, double lon1, double lat2, double lon2) noexcept {
    double dLat = (lat2 - lat1) * DEG2RAD;
    double dLon = (lon2 - lon1) * DEG2RAD;
    double rLat1 = lat1 * DEG2RAD;
    double rLat2 = lat2 * DEG2RAD;

    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(rLat1) * std::cos(rLat2) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(std::max(0.0, 1.0 - a)));
    return EARTH_RADIUS_METERS * c;
}
} // namespace

DigitalElevationModel::DigitalElevationModel(
    GeoBounds bounds,
    size_t rows,
    size_t cols,
    std::vector<float> elevationData
) : bounds_(bounds), rows_(rows), cols_(cols), elevationData_(std::move(elevationData)) {
    if (rows_ < 2 || cols_ < 2) {
        throw std::invalid_argument("DEM dimensions must be at least 2x2.");
    }
    if (elevationData_.size() != rows_ * cols_) {
        throw std::invalid_argument("Elevation data buffer does not match rows * cols.");
    }

    cellLatSize_ = (bounds_.maxLat - bounds_.minLat) / static_cast<double>(rows_ - 1);
    cellLonSize_ = (bounds_.maxLon - bounds_.minLon) / static_cast<double>(cols_ - 1);

    double midLat = (bounds_.minLat + bounds_.maxLat) * 0.5;
    cellMetersY_ = haversineDistanceMeters(midLat, bounds_.minLon, midLat + cellLatSize_, bounds_.minLon);
    cellMetersX_ = haversineDistanceMeters(midLat, bounds_.minLon, midLat, bounds_.minLon + cellLonSize_);
}

float DigitalElevationModel::getRawElevation(size_t r, size_t c) const noexcept {
    r = std::min(r, rows_ - 1);
    c = std::min(c, cols_ - 1);
    return elevationData_[index(r, c)];
}

double DigitalElevationModel::getElevationAt(double lat, double lon) const noexcept {
    if (lat < bounds_.minLat || lat > bounds_.maxLat ||
        lon < bounds_.minLon || lon > bounds_.maxLon) {
        return 0.0;
    }

    // Grid coordinates
    double normR = (lat - bounds_.minLat) / cellLatSize_;
    double normC = (lon - bounds_.minLon) / cellLonSize_;

    size_t r0 = static_cast<size_t>(std::clamp(std::floor(normR), 0.0, static_cast<double>(rows_ - 2)));
    size_t c0 = static_cast<size_t>(std::clamp(std::floor(normC), 0.0, static_cast<double>(cols_ - 2)));
    size_t r1 = r0 + 1;
    size_t c1 = c0 + 1;

    double dr = normR - static_cast<double>(r0);
    double dc = normC - static_cast<double>(c0);

    double z00 = getRawElevation(r0, c0);
    double z01 = getRawElevation(r0, c1);
    double z10 = getRawElevation(r1, c0);
    double z11 = getRawElevation(r1, c1);

    // Bilinear interpolation
    double z0 = z00 * (1.0 - dc) + z01 * dc;
    double z1 = z10 * (1.0 - dc) + z11 * dc;
    return z0 * (1.0 - dr) + z1 * dr;
}

void DigitalElevationModel::computeGradients(size_t r, size_t c, double& dzDx, double& dzDy) const noexcept {
    size_t rMin = (r > 0) ? r - 1 : 0;
    size_t rMax = (r + 1 < rows_) ? r + 1 : rows_ - 1;
    size_t cMin = (c > 0) ? c - 1 : 0;
    size_t cMax = (c + 1 < cols_) ? c + 1 : cols_ - 1;

    // Horn 3x3 grid weights
    // a b c
    // d e f
    // g h i
    double a = getRawElevation(rMin, cMin);
    double b = getRawElevation(rMin, c);
    double cVal = getRawElevation(rMin, cMax);
    double d = getRawElevation(r, cMin);
    double f = getRawElevation(r, cMax);
    double g = getRawElevation(rMax, cMin);
    double h = getRawElevation(rMax, c);
    double i = getRawElevation(rMax, cMax);

    dzDx = ((cVal + 2.0 * f + i) - (a + 2.0 * d + g)) / (8.0 * cellMetersX_);
    dzDy = ((g + 2.0 * h + i) - (a + 2.0 * b + cVal)) / (8.0 * cellMetersY_);
}

double DigitalElevationModel::getSlopeDegrees(double lat, double lon) const noexcept {
    if (lat < bounds_.minLat || lat > bounds_.maxLat ||
        lon < bounds_.minLon || lon > bounds_.maxLon) {
        return 0.0;
    }

    size_t r = static_cast<size_t>(std::clamp(std::round((lat - bounds_.minLat) / cellLatSize_), 0.0, static_cast<double>(rows_ - 1)));
    size_t c = static_cast<size_t>(std::clamp(std::round((lon - bounds_.minLon) / cellLonSize_), 0.0, static_cast<double>(cols_ - 1)));

    double dzDx, dzDy;
    computeGradients(r, c, dzDx, dzDy);
    double slopeRad = std::atan(std::sqrt(dzDx * dzDx + dzDy * dzDy));
    return slopeRad * RAD2DEG;
}

double DigitalElevationModel::getAspectDegrees(double lat, double lon) const noexcept {
    if (lat < bounds_.minLat || lat > bounds_.maxLat ||
        lon < bounds_.minLon || lon > bounds_.maxLon) {
        return 0.0;
    }

    size_t r = static_cast<size_t>(std::clamp(std::round((lat - bounds_.minLat) / cellLatSize_), 0.0, static_cast<double>(rows_ - 1)));
    size_t c = static_cast<size_t>(std::clamp(std::round((lon - bounds_.minLon) / cellLonSize_), 0.0, static_cast<double>(cols_ - 1)));

    double dzDx, dzDy;
    computeGradients(r, c, dzDx, dzDy);

    if (std::abs(dzDx) < 1e-6 && std::abs(dzDy) < 1e-6) {
        return -1.0; // Flat terrain has no aspect
    }

    double aspectRad = std::atan2(dzDy, -dzDx);
    double aspectDeg = aspectRad * RAD2DEG;
    if (aspectDeg < 0.0) {
        aspectDeg = 90.0 - aspectDeg;
    } else if (aspectDeg > 90.0) {
        aspectDeg = 360.0 - aspectDeg + 90.0;
    } else {
        aspectDeg = 90.0 - aspectDeg;
    }
    return std::fmod(aspectDeg + 360.0, 360.0);
}

double DigitalElevationModel::computeHillshade(
    double lat, double lon,
    double sunAzimuthDeg,
    double sunAltitudeDeg
) const noexcept {
    double slopeDeg = getSlopeDegrees(lat, lon);
    double aspectDeg = getAspectDegrees(lat, lon);
    if (aspectDeg < 0.0) aspectDeg = 0.0;

    double zenithRad = (90.0 - sunAltitudeDeg) * DEG2RAD;
    double azimuthRad = sunAzimuthDeg * DEG2RAD;
    double slopeRad = slopeDeg * DEG2RAD;
    double aspectRad = aspectDeg * DEG2RAD;

    double shade = (std::cos(zenithRad) * std::cos(slopeRad)) +
                   (std::sin(zenithRad) * std::sin(slopeRad) * std::cos(azimuthRad - aspectRad));

    return std::clamp(shade, 0.0, 1.0);
}

std::vector<HelicopterLandingZone> DigitalElevationModel::findLandingZones(
    double minClearanceRadiusMeters,
    double maxAllowableSlopeDeg,
    size_t maxResults
) const {
    std::vector<HelicopterLandingZone> candidates;

    int radiusCellsX = std::max(1, static_cast<int>(std::ceil(minClearanceRadiusMeters / cellMetersX_)));
    int radiusCellsY = std::max(1, static_cast<int>(std::ceil(minClearanceRadiusMeters / cellMetersY_)));
    int step = std::max(1, std::min(radiusCellsX, radiusCellsY));

    for (size_t r = radiusCellsY; r < rows_ - radiusCellsY; r += step) {
        for (size_t c = radiusCellsX; c < cols_ - radiusCellsX; c += step) {
            double centerLat = bounds_.minLat + r * cellLatSize_;
            double centerLon = bounds_.minLon + c * cellLonSize_;

            double sumElev = 0.0;
            double sumElev2 = 0.0;
            double maxSlope = 0.0;
            int count = 0;
            bool obstacleDetected = false;

            double centerElev = getRawElevation(r, c);

            for (int dr = -radiusCellsY; dr <= radiusCellsY; ++dr) {
                for (int dc = -radiusCellsX; dc <= radiusCellsX; ++dc) {
                    double distMeters = std::sqrt(
                        (dr * cellMetersY_) * (dr * cellMetersY_) +
                        (dc * cellMetersX_) * (dc * cellMetersX_)
                    );
                    if (distMeters <= minClearanceRadiusMeters) {
                        size_t sampleR = static_cast<size_t>(static_cast<int>(r) + dr);
                        size_t sampleC = static_cast<size_t>(static_cast<int>(c) + dc);
                        double elev = getRawElevation(sampleR, sampleC);

                        // If elevation discrepancy exceeds 2.5m within landing circle, it's an obstacle
                        if (std::abs(elev - centerElev) > 2.5) {
                            obstacleDetected = true;
                            break;
                        }

                        double dzDx, dzDy;
                        computeGradients(sampleR, sampleC, dzDx, dzDy);
                        double slope = std::atan(std::sqrt(dzDx * dzDx + dzDy * dzDy)) * RAD2DEG;
                        if (slope > maxSlope) maxSlope = slope;

                        sumElev += elev;
                        sumElev2 += elev * elev;
                        count++;
                    }
                }
                if (obstacleDetected) break;
            }

            if (obstacleDetected || count == 0 || maxSlope > maxAllowableSlopeDeg) {
                continue;
            }

            double meanElev = sumElev / count;
            double variance = std::max(0.0, (sumElev2 / count) - (meanElev * meanElev));
            double stdDev = std::sqrt(variance);

            // Roughness penalty
            if (stdDev > 0.50) continue;

            // Score: flatter, smoother terrain gets higher suitability
            double slopeScore = 1.0 - (maxSlope / maxAllowableSlopeDeg);
            double roughnessScore = 1.0 - (stdDev / 0.50);
            double suitability = 0.6 * slopeScore + 0.4 * roughnessScore;

            // Recommended approach azimuth: away from high terrain
            double dzDx, dzDy;
            computeGradients(r, c, dzDx, dzDy);
            double approachAzimuth = std::fmod(RAD2DEG * std::atan2(-dzDy, dzDx) + 360.0, 360.0);

            candidates.push_back({
                centerLat,
                centerLon,
                meanElev,
                maxSlope,
                stdDev,
                minClearanceRadiusMeters,
                approachAzimuth,
                suitability
            });
        }
    }

    // Sort by suitability descending
    std::sort(candidates.begin(), candidates.end(), [](const HelicopterLandingZone& a, const HelicopterLandingZone& b) {
        return a.suitabilityScore > b.suitabilityScore;
    });

    if (candidates.size() > maxResults) {
        candidates.resize(maxResults);
    }

    return candidates;
}

} // namespace georescux::spatial
