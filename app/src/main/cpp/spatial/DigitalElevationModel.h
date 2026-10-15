#ifndef GEORESCUX_DIGITAL_ELEVATION_MODEL_H
#define GEORESCUX_DIGITAL_ELEVATION_MODEL_H

#include <vector>
#include <string>
#include <cstddef>
#include <cmath>

namespace georescux::spatial {

struct GeoBounds {
    double minLat;
    double maxLat;
    double minLon;
    double maxLon;
};

struct TerrainCellMetrics {
    double elevationMeters;
    double slopeDegrees;
    double aspectDegrees;
    double hillshade; // 0.0 - 1.0 illumination factor
    bool isSafeForFoot; // slope < 35 degrees
};

struct HelicopterLandingZone {
    double latitude;
    double longitude;
    double elevationMeters;
    double meanSlopeDegrees;
    double roughnessStdDev;
    double clearanceRadiusMeters;
    double recommendedApproachAzimuthDeg;
    double suitabilityScore; // 0.0 (unusable) to 1.0 (ideal)
};

/**
 * High-performance 2D Digital Elevation Model (DEM) and terrain analysis engine.
 * Computes Horn's slope, aspect, solar hillshading, and identifies
 * emergency Helicopter Landing Zones (HLZ) in mountainous disaster zones.
 */
class DigitalElevationModel {
public:
    DigitalElevationModel(
        GeoBounds bounds,
        size_t rows,
        size_t cols,
        std::vector<float> elevationData
    );

    /**
     * Gets bilinearly interpolated elevation at arbitrary (lat, lon).
     */
    [[nodiscard]] double getElevationAt(double lat, double lon) const noexcept;

    /**
     * Computes slope in degrees using Horn's 8-neighbor algorithm.
     */
    [[nodiscard]] double getSlopeDegrees(double lat, double lon) const noexcept;

    /**
     * Computes aspect (compass direction the slope faces in degrees: 0=North, 90=East, 180=South, 270=West).
     */
    [[nodiscard]] double getAspectDegrees(double lat, double lon) const noexcept;

    /**
     * Computes hillshade illumination factor [0.0, 1.0] given sun azimuth and altitude.
     */
    [[nodiscard]] double computeHillshade(
        double lat, double lon,
        double sunAzimuthDeg = 315.0,
        double sunAltitudeDeg = 45.0
    ) const noexcept;

    /**
     * Scans the DEM for viable emergency Helicopter Landing Zones (HLZ)
     * suitable for IAF Mi-17 and Cheetah/Dhruv disaster airlift operations.
     */
    [[nodiscard]] std::vector<HelicopterLandingZone> findLandingZones(
        double minClearanceRadiusMeters = 25.0,
        double maxAllowableSlopeDeg = 8.0,
        size_t maxResults = 10
    ) const;

    [[nodiscard]] const GeoBounds& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] size_t getRows() const noexcept { return rows_; }
    [[nodiscard]] size_t getCols() const noexcept { return cols_; }

private:
    GeoBounds bounds_;
    size_t rows_;
    size_t cols_;
    std::vector<float> elevationData_;
    double cellLatSize_;
    double cellLonSize_;
    double cellMetersX_;
    double cellMetersY_;

    [[nodiscard]] inline size_t index(size_t r, size_t c) const noexcept {
        return r * cols_ + c;
    }

    [[nodiscard]] float getRawElevation(size_t r, size_t c) const noexcept;
    void computeGradients(size_t r, size_t c, double& dzDx, double& dzDy) const noexcept;
};

} // namespace georescux::spatial

#endif // GEORESCUX_DIGITAL_ELEVATION_MODEL_H
