#ifndef GEORESCUX_DEAD_RECKONING_ENGINE_H
#define GEORESCUX_DEAD_RECKONING_ENGINE_H

#include <vector>
#include <cstdint>
#include <cmath>

namespace georescux::telemetry {

struct ImuSample {
    uint64_t timestampMs;
    float accelX, accelY, accelZ; // m/s^2
    float gyroX, gyroY, gyroZ;    // rad/s
    float magX, magY, magZ;       // microTesla
};

struct DeadReckoningEstimate {
    double latitude;
    double longitude;
    double altitudeMeters;
    double headingDegrees; // 0=North, 90=East
    double totalDistanceMeters;
    uint32_t stepCount;
    bool isGpsValid;
    double estimatedAccuracyMeters;
};

/**
 * Pedestrian Dead Reckoning (PDR) and Sensor Fusion Engine.
 * Provides continuous tracking during GPS outages in tunnels, collapsed structures,
 * and deep Himalayan ravines using IMU step detection, Weinberg step length,
 * and complementary orientation filtering.
 */
class DeadReckoningEngine {
public:
    DeadReckoningEngine(double initialLat, double initialLon, double initialAltitude = 0.0);

    /**
     * Updates engine with fresh external GPS fix when available.
     * Re-anchors dead-reckoning trajectory and resets accumulated drift.
     */
    void updateGpsAnchor(double lat, double lon, double alt, double accuracyMeters);

    /**
     * Ingests a high-frequency IMU sample (accel, gyro, mag).
     * Returns true if a new step was detected and position propagated.
     */
    bool processImuSample(const ImuSample& sample);

    /**
     * Gets latest estimated position and metrics.
     */
    [[nodiscard]] DeadReckoningEstimate getCurrentEstimate() const noexcept { return currentEstimate_; }

    /**
     * Sets user calibration height/stride factor for Weinberg formula.
     */
    void setWeinbergCalibrationConstant(double k) noexcept { weinbergK_ = k; }

private:
    DeadReckoningEstimate currentEstimate_;
    double weinbergK_ = 0.415; // standard Weinberg calibration constant
    uint64_t lastStepTimestampMs_ = 0;
    uint64_t lastImuTimestampMs_ = 0;

    // Moving window peak/valley detection
    float windowAccelMax_ = 9.81f;
    float windowAccelMin_ = 9.81f;
    float prevAccelMagnitude_ = 9.81f;
    bool isRising_ = false;

    // Orientation state (complementary filter)
    float pitchRad_ = 0.0f;
    float rollRad_ = 0.0f;
    float yawRad_ = 0.0f;

    void updateOrientation(const ImuSample& sample, float dtSec);
    bool detectStep(const ImuSample& sample);
    double estimateStepLength(float aMax, float aMin) const noexcept;
    void propagatePosition(double stepLengthMeters, double headingDeg);
};

} // namespace georescux::telemetry

#endif // GEORESCUX_DEAD_RECKONING_ENGINE_H
