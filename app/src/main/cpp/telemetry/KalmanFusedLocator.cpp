#include "KalmanFusedLocator.h"
#include <algorithm>
#include <cmath>

namespace georescux::telemetry {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double METERS_PER_DEG_LAT = 111139.0;
} // namespace

KalmanFusedLocator::KalmanFusedLocator(double originLat, double originLon, double originAltitude)
    : originLat_(originLat), originLon_(originLon), currentAltitude_(originAltitude) {
    initializeCovariance();
}

void KalmanFusedLocator::initializeCovariance() {
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            P_[i][j] = 0.0;
        }
    }
    P_[0][0] = 25.0;  // Pos variance (m2)
    P_[1][1] = 25.0;
    P_[2][2] = 4.0;   // Vel variance (m2/s2)
    P_[3][3] = 4.0;
    P_[4][4] = 0.04;  // Bias variance ((m/s2)2)
    P_[5][5] = 0.04;
}

void KalmanFusedLocator::resetOrigin(double lat, double lon, double alt) {
    originLat_ = lat;
    originLon_ = lon;
    currentAltitude_ = alt;
    for (int i = 0; i < 6; ++i) x_[i] = 0.0;
    initializeCovariance();
    lastTimestampMs_ = 0;
}

void KalmanFusedLocator::gpsToLocalMeters(double lat, double lon, double& outX, double& outY) const noexcept {
    double dLat = lat - originLat_;
    double dLon = lon - originLon_;
    outY = dLat * METERS_PER_DEG_LAT;
    outX = dLon * (METERS_PER_DEG_LAT * std::cos(originLat_ * DEG2RAD));
}

void KalmanFusedLocator::localMetersToGps(double xMeters, double yMeters, double& outLat, double& outLon) const noexcept {
    outLat = originLat_ + (yMeters / METERS_PER_DEG_LAT);
    outLon = originLon_ + (xMeters / (METERS_PER_DEG_LAT * std::cos(originLat_ * DEG2RAD)));
}

void KalmanFusedLocator::predict(float accelX, float accelY, uint64_t timestampMs) {
    if (lastTimestampMs_ == 0 || timestampMs <= lastTimestampMs_) {
        lastTimestampMs_ = timestampMs;
        return;
    }

    double dt = static_cast<double>(timestampMs - lastTimestampMs_) / 1000.0;
    lastTimestampMs_ = timestampMs;
    if (dt > 1.0) dt = 0.1; // clamp large jumps

    double ax = static_cast<double>(accelX) - x_[4];
    double ay = static_cast<double>(accelY) - x_[5];

    // State propagation
    x_[0] += x_[2] * dt + 0.5 * ax * dt * dt;
    x_[1] += x_[3] * dt + 0.5 * ay * dt * dt;
    x_[2] += ax * dt;
    x_[3] += ay * dt;

    // F matrix:
    // [1 0 dt 0 -0.5dt^2 0]
    // [0 1 0 dt 0 -0.5dt^2]
    // [0 0 1 0 -dt 0]
    // [0 0 0 1 0 -dt]
    // [0 0 0 0 1 0]
    // [0 0 0 0 0 1]
    double F[6][6] = {{0.0}};
    for (int i = 0; i < 6; ++i) F[i][i] = 1.0;
    F[0][2] = dt; F[0][4] = -0.5 * dt * dt;
    F[1][3] = dt; F[1][5] = -0.5 * dt * dt;
    F[2][4] = -dt;
    F[3][5] = -dt;

    // P = F * P * F^T + Q
    double FP[6][6] = {{0.0}};
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            for (int k = 0; k < 6; ++k) {
                FP[r][c] += F[r][k] * P_[k][c];
            }
        }
    }

    double nextP[6][6] = {{0.0}};
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            for (int k = 0; k < 6; ++k) {
                nextP[r][c] += FP[r][k] * F[c][k];
            }
        }
    }

    // Add process noise Q
    nextP[0][0] += qPos_ * dt;
    nextP[1][1] += qPos_ * dt;
    nextP[2][2] += qVel_ * dt;
    nextP[3][3] += qVel_ * dt;
    nextP[4][4] += qBias_ * dt;
    nextP[5][5] += qBias_ * dt;

    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            P_[r][c] = nextP[r][c];
        }
    }
}

void KalmanFusedLocator::updateGps(double lat, double lon, double accuracyMeters, uint64_t timestampMs) {
    double zx, zy;
    gpsToLocalMeters(lat, lon, zx, zy);

    double rVar = std::max(4.0, accuracyMeters * accuracyMeters);

    // Measurement residual y = z - Hx
    double y0 = zx - x_[0];
    double y1 = zy - x_[1];

    // S = H P H^T + R: 2x2 matrix
    double s00 = P_[0][0] + rVar;
    double s01 = P_[0][1];
    double s10 = P_[1][0];
    double s11 = P_[1][1] + rVar;

    double detS = s00 * s11 - s01 * s10;
    if (std::abs(detS) < 1e-9) return;

    // Inverse of S
    double invS00 = s11 / detS;
    double invS01 = -s01 / detS;
    double invS10 = -s10 / detS;
    double invS11 = s00 / detS;

    // Kalman Gain K = P * H^T * invS (6x2 matrix)
    double K[6][2] = {{0.0}};
    for (int i = 0; i < 6; ++i) {
        double ph0 = P_[i][0];
        double ph1 = P_[i][1];
        K[i][0] = ph0 * invS00 + ph1 * invS10;
        K[i][1] = ph0 * invS01 + ph1 * invS11;
    }

    // State update x = x + K * y
    for (int i = 0; i < 6; ++i) {
        x_[i] += K[i][0] * y0 + K[i][1] * y1;
    }

    // Covariance update P = (I - K H) P
    double I_KH[6][6] = {{0.0}};
    for (int i = 0; i < 6; ++i) I_KH[i][i] = 1.0;
    for (int i = 0; i < 6; ++i) {
        I_KH[i][0] -= K[i][0];
        I_KH[i][1] -= K[i][1];
    }

    double newP[6][6] = {{0.0}};
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            for (int k = 0; k < 6; ++k) {
                newP[r][c] += I_KH[r][k] * P_[k][c];
            }
        }
    }

    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            P_[r][c] = newP[r][c];
        }
    }

    lastTimestampMs_ = timestampMs;
}

void KalmanFusedLocator::updateBarometricAltitude(double altitudeMeters, double varianceM2) {
    // 1D Kalman filter update on altitude
    double rVar = std::max(0.5, varianceM2);
    double pAlt = 4.0;
    double kGain = pAlt / (pAlt + rVar);
    currentAltitude_ = currentAltitude_ + kGain * (altitudeMeters - currentAltitude_);
}

void KalmanFusedLocator::updateBeaconProximity(double beaconLat, double beaconLon, double rssiDistanceMeters) {
    // Beacon provides anchor fix with accuracy proportional to RSSI distance
    updateGps(beaconLat, beaconLon, std::max(3.0, rssiDistanceMeters), lastTimestampMs_);
}

FusedLocationEstimate KalmanFusedLocator::getEstimate() const noexcept {
    FusedLocationEstimate est{};
    localMetersToGps(x_[0], x_[1], est.latitude, est.longitude);
    est.altitudeMeters = currentAltitude_;

    double vx = x_[2];
    double vy = x_[3];
    est.speedMps = std::sqrt(vx * vx + vy * vy);

    if (est.speedMps > 0.2) {
        double bearingRad = std::atan2(vx, vy); // North is Y, East is X
        est.bearingDegrees = std::fmod(bearingRad * RAD2DEG + 360.0, 360.0);
    } else {
        est.bearingDegrees = 0.0;
    }

    est.horizontalAccuracyMeters = std::sqrt(P_[0][0] + P_[1][1]);
    est.timestampMs = lastTimestampMs_;
    return est;
}

} // namespace georescux::telemetry
