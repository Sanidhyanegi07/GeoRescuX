#include "DeadReckoningEngine.h"
#include <algorithm>
#include <cmath>

namespace georescux::telemetry {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG2RAD = PI / 180.0;
constexpr double RAD2DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_M = 6371008.8;
} // namespace

DeadReckoningEngine::DeadReckoningEngine(double initialLat, double initialLon, double initialAltitude) {
    currentEstimate_.latitude = initialLat;
    currentEstimate_.longitude = initialLon;
    currentEstimate_.altitudeMeters = initialAltitude;
    currentEstimate_.headingDegrees = 0.0;
    currentEstimate_.totalDistanceMeters = 0.0;
    currentEstimate_.stepCount = 0;
    currentEstimate_.isGpsValid = true;
    currentEstimate_.estimatedAccuracyMeters = 5.0;
}

void DeadReckoningEngine::updateGpsAnchor(double lat, double lon, double alt, double accuracyMeters) {
    currentEstimate_.latitude = lat;
    currentEstimate_.longitude = lon;
    currentEstimate_.altitudeMeters = alt;
    currentEstimate_.isGpsValid = true;
    currentEstimate_.estimatedAccuracyMeters = accuracyMeters;
}

void DeadReckoningEngine::updateOrientation(const ImuSample& sample, float dtSec) {
    if (dtSec <= 0.0f || dtSec > 0.5f) dtSec = 0.02f; // default 50Hz (20ms)

    // Accelerometer tilt
    float aX = sample.accelX;
    float aY = sample.accelY;
    float aZ = sample.accelZ;

    float pitchAcc = std::atan2(aY, std::sqrt(aX * aX + aZ * aZ));
    float rollAcc = std::atan2(-aX, aZ);

    // Gyroscope integration
    pitchRad_ += sample.gyroX * dtSec;
    rollRad_ += sample.gyroY * dtSec;
    yawRad_ += sample.gyroZ * dtSec;

    // Complementary filter for pitch & roll
    constexpr float ALPHA_TILT = 0.96f;
    pitchRad_ = ALPHA_TILT * pitchRad_ + (1.0f - ALPHA_TILT) * pitchAcc;
    rollRad_ = ALPHA_TILT * rollRad_ + (1.0f - ALPHA_TILT) * rollAcc;

    // Tilt-compensated magnetometer yaw
    float mX = sample.magX;
    float mY = sample.magY;
    float mZ = sample.magZ;

    float sinP = std::sin(pitchRad_);
    float cosP = std::cos(pitchRad_);
    float sinR = std::sin(rollRad_);
    float cosR = std::cos(rollRad_);

    float bXh = mX * cosP + mZ * sinP;
    float bYh = mX * sinR * sinP + mY * cosR - mZ * sinR * cosP;

    float magYaw = std::atan2(-bYh, bXh);

    // Complementary filter for yaw
    constexpr float ALPHA_YAW = 0.94f;
    yawRad_ = ALPHA_YAW * yawRad_ + (1.0f - ALPHA_YAW) * magYaw;

    double headingDeg = static_cast<double>(yawRad_ * RAD2DEG);
    currentEstimate_.headingDegrees = std::fmod(headingDeg + 360.0, 360.0);
}

bool DeadReckoningEngine::detectStep(const ImuSample& sample) {
    float mag = std::sqrt(
        sample.accelX * sample.accelX +
        sample.accelY * sample.accelY +
        sample.accelZ * sample.accelZ
    );

    if (mag > windowAccelMax_) windowAccelMax_ = mag;
    if (mag < windowAccelMin_) windowAccelMin_ = mag;

    bool stepDetected = false;

    // Peak detection
    if (isRising_ && mag < prevAccelMagnitude_) {
        // Transitioned from rise to fall (local peak)
        float diff = windowAccelMax_ - windowAccelMin_;
        constexpr float MIN_PEAK_VALLEY_DIFF = 1.8f; // m/s^2

        if (diff >= MIN_PEAK_VALLEY_DIFF) {
            uint64_t elapsed = sample.timestampMs - lastStepTimestampMs_;
            constexpr uint64_t REFRACTORY_PERIOD_MS = 240; // max ~4 steps/sec

            if (elapsed >= REFRACTORY_PERIOD_MS) {
                stepDetected = true;
                lastStepTimestampMs_ = sample.timestampMs;
            }
        }

        // Reset dynamic bounds
        windowAccelMax_ = mag;
        windowAccelMin_ = mag;
        isRising_ = false;
    } else if (!isRising_ && mag > prevAccelMagnitude_) {
        isRising_ = true;
    }

    prevAccelMagnitude_ = mag;
    return stepDetected;
}

double DeadReckoningEngine::estimateStepLength(float aMax, float aMin) const noexcept {
    float diff = std::max(0.1f, aMax - aMin);
    // Weinberg formula: L = K * (aMax - aMin)^(1/4)
    double stepLength = weinbergK_ * std::pow(static_cast<double>(diff), 0.25);
    return std::clamp(stepLength, 0.40, 1.20); // clamp realistic human step between 40cm and 120cm
}

void DeadReckoningEngine::propagatePosition(double stepLengthMeters, double headingDeg) {
    double d = stepLengthMeters;
    double r = EARTH_RADIUS_M;
    double theta = headingDeg * DEG2RAD;

    double lat1 = currentEstimate_.latitude * DEG2RAD;
    double lon1 = currentEstimate_.longitude * DEG2RAD;

    double sinLat1 = std::sin(lat1);
    double cosLat1 = std::cos(lat1);
    double sinDR = std::sin(d / r);
    double cosDR = std::cos(d / r);

    double lat2 = std::asin(sinLat1 * cosDR + cosLat1 * sinDR * std::cos(theta));
    double lon2 = lon1 + std::atan2(
        std::sin(theta) * sinDR * cosLat1,
        cosDR - sinLat1 * std::sin(lat2)
    );

    currentEstimate_.latitude = lat2 * RAD2DEG;
    currentEstimate_.longitude = lon2 * RAD2DEG;
    currentEstimate_.totalDistanceMeters += stepLengthMeters;
    currentEstimate_.stepCount++;

    // When GPS is lost, dead reckoning accumulates drift error ~ 5% of distance traveled
    if (!currentEstimate_.isGpsValid) {
        currentEstimate_.estimatedAccuracyMeters += 0.05 * stepLengthMeters;
    }
}

bool DeadReckoningEngine::processImuSample(const ImuSample& sample) {
    float dtSec = 0.02f;
    if (lastImuTimestampMs_ > 0 && sample.timestampMs > lastImuTimestampMs_) {
        dtSec = static_cast<float>(sample.timestampMs - lastImuTimestampMs_) / 1000.0f;
    }
    lastImuTimestampMs_ = sample.timestampMs;

    updateOrientation(sample, dtSec);

    if (detectStep(sample)) {
        double stepLen = estimateStepLength(windowAccelMax_, windowAccelMin_);
        propagatePosition(stepLen, currentEstimate_.headingDegrees);
        return true;
    }

    return false;
}

} // namespace georescux::telemetry
