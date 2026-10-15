#ifndef GEORESCUX_KALMAN_FUSED_LOCATOR_H
#define GEORESCUX_KALMAN_FUSED_LOCATOR_H

#include <vector>
#include <cstdint>
#include <cmath>

namespace georescux::telemetry {

struct FusedLocationEstimate {
    double latitude;
    double longitude;
    double altitudeMeters;
    double speedMps;
    double bearingDegrees;
    double horizontalAccuracyMeters;
    uint64_t timestampMs;
};

/**
 * 6-State Extended Kalman Filter (EKF) Multi-Sensor Fusion Engine.
 * Fuses asynchronous GPS coordinates, IMU accelerations, barometric pressure,
 * and BLE beacon proximity into an optimal, continuous position trajectory
 * with automatic accelerometer bias estimation and covariance tracking.
 */
class KalmanFusedLocator {
public:
    KalmanFusedLocator(double originLat, double originLon, double originAltitude = 0.0);

    /**
     * Prediction step: updates state using high-frequency accelerometer sample.
     */
    void predict(float accelX, float accelY, uint64_t timestampMs);

    /**
     * Measurement update: ingests external GPS position fix.
     */
    void updateGps(double lat, double lon, double accuracyMeters, uint64_t timestampMs);

    /**
     * Measurement update: ingests barometric altitude measurement.
     */
    void updateBarometricAltitude(double altitudeMeters, double varianceM2);

    /**
     * Measurement update: ingests known fixed BLE rescue beacon proximity.
     */
    void updateBeaconProximity(double beaconLat, double beaconLon, double rssiDistanceMeters);

    /**
     * Returns current fused location estimate.
     */
    [[nodiscard]] FusedLocationEstimate getEstimate() const noexcept;

    /**
     * Resets origin coordinates.
     */
    void resetOrigin(double lat, double lon, double alt = 0.0);

private:
    double originLat_;
    double originLon_;
    double currentAltitude_;
    uint64_t lastTimestampMs_ = 0;

    // State vector: [x, y, vx, vy, bax, bay]
    double x_[6] = {0.0};

    // Error covariance matrix: 6x6
    double P_[6][6] = {{0.0}};

    // Process noise Q and measurement noise R
    double qPos_ = 0.1;
    double qVel_ = 0.5;
    double qBias_ = 0.01;

    void initializeCovariance();
    void localMetersToGps(double xMeters, double yMeters, double& outLat, double& outLon) const noexcept;
    void gpsToLocalMeters(double lat, double lon, double& outX, double& outY) const noexcept;
};

} // namespace georescux::telemetry

#endif // GEORESCUX_KALMAN_FUSED_LOCATOR_H
