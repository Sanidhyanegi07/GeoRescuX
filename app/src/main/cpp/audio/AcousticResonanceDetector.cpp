#include "AcousticResonanceDetector.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace georescux::audio {

namespace {
constexpr double PI = 3.14159265358979323846;

bool isPowerOfTwo(size_t n) noexcept {
    return (n > 0) && ((n & (n - 1)) == 0);
}
} // namespace

AcousticResonanceDetector::AcousticResonanceDetector(size_t sampleRateHz, size_t fftSize)
    : sampleRateHz_(sampleRateHz), fftSize_(fftSize) {
    if (!isPowerOfTwo(fftSize_)) {
        fftSize_ = 1024;
    }
    initializeWindow();
    audioBuffer_.reserve(sampleRateHz_ * 5); // 5-second sliding buffer
}

void AcousticResonanceDetector::initializeWindow() {
    hanningWindow_.resize(fftSize_);
    for (size_t i = 0; i < fftSize_; ++i) {
        hanningWindow_[i] = 0.5 * (1.0 - std::cos((2.0 * PI * i) / (fftSize_ - 1)));
    }
}

size_t AcousticResonanceDetector::bitReverse(size_t n, size_t bits) noexcept {
    size_t reversed = 0;
    for (size_t i = 0; i < bits; ++i) {
        if ((n >> i) & 1) {
            reversed |= (1ULL << (bits - 1 - i));
        }
    }
    return reversed;
}

void AcousticResonanceDetector::computeFft(std::vector<std::complex<double>>& buffer, bool inverse) {
    size_t n = buffer.size();
    if (n <= 1) return;

    size_t bits = static_cast<size_t>(std::round(std::log2(n)));

    // Bit-reversal permutation
    for (size_t i = 0; i < n; ++i) {
        size_t rev = bitReverse(i, bits);
        if (i < rev) {
            std::swap(buffer[i], buffer[rev]);
        }
    }

    // Cooley-Tukey butterfly stages
    for (size_t len = 2; len <= n; len <<= 1) {
        double angle = 2.0 * PI / len * (inverse ? -1.0 : 1.0);
        std::complex<double> wlen(std::cos(angle), std::sin(angle));

        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<double> u = buffer[i + j];
                std::complex<double> v = buffer[i + j + len / 2] * w;
                buffer[i + j] = u + v;
                buffer[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        for (auto& x : buffer) {
            x /= static_cast<double>(n);
        }
    }
}

void AcousticResonanceDetector::processPcmAudio(
    const int16_t* pcmSamples,
    size_t sampleCount,
    uint64_t startTimestampMs
) {
    if (pcmSamples == nullptr || sampleCount == 0) return;

    // Convert to normalized floating point [-1.0, 1.0]
    double sumSq = 0.0;
    for (size_t i = 0; i < sampleCount; ++i) {
        double s = static_cast<double>(pcmSamples[i]) / 32768.0;
        audioBuffer_.push_back(s);
        sumSq += s * s;
    }

    // Retain only last 5 seconds of samples
    size_t maxCapacity = sampleRateHz_ * 5;
    if (audioBuffer_.size() > maxCapacity) {
        audioBuffer_.erase(audioBuffer_.begin(), audioBuffer_.begin() + (audioBuffer_.size() - maxCapacity));
    }

    // Transient detection: short-term energy vs long-term baseline
    size_t windowLen = sampleRateHz_ / 50; // 20ms frame
    if (sampleCount >= windowLen) {
        for (size_t i = 0; i + windowLen <= sampleCount; i += windowLen / 2) {
            double frameEnergy = 0.0;
            for (size_t k = 0; k < windowLen; ++k) {
                double val = static_cast<double>(pcmSamples[i + k]) / 32768.0;
                frameEnergy += val * val;
            }
            frameEnergy /= windowLen;

            // Sudden transient threshold
            if (frameEnergy > 0.02) {
                uint64_t tapTime = startTimestampMs + static_cast<uint64_t>((i * 1000) / sampleRateHz_);

                // Refractory period: ignore taps within 120ms
                if (detectedTaps_.empty() || (tapTime - detectedTaps_.back().timestampMs > 120)) {
                    double confidence = std::min(1.0, frameEnergy * 15.0);
                    detectedTaps_.push_back({tapTime, std::sqrt(frameEnergy), confidence});
                }
            }
        }
    }

    // Keep at most last 100 tap events
    if (detectedTaps_.size() > 100) {
        detectedTaps_.erase(detectedTaps_.begin(), detectedTaps_.begin() + (detectedTaps_.size() - 100));
    }
}

std::vector<SpectralPeak> AcousticResonanceDetector::getResonantPeaks(
    double minThresholdDb,
    size_t maxPeaks
) const {
    std::vector<SpectralPeak> peaks;
    if (audioBuffer_.size() < fftSize_) {
        return peaks;
    }

    // Extract latest fftSize_ samples
    std::vector<std::complex<double>> fftBuf(fftSize_);
    size_t offset = audioBuffer_.size() - fftSize_;
    for (size_t i = 0; i < fftSize_; ++i) {
        fftBuf[i] = audioBuffer_[offset + i] * hanningWindow_[i];
    }

    computeFft(fftBuf, false);

    // Compute power spectrum in dB
    size_t halfSize = fftSize_ / 2;
    std::vector<double> powerDb(halfSize);
    for (size_t i = 0; i < halfSize; ++i) {
        double mag = std::abs(fftBuf[i]) / (fftSize_ / 2.0);
        powerDb[i] = (mag > 1e-9) ? 20.0 * std::log10(mag) : -100.0;
    }

    // Find local maxima
    for (size_t i = 2; i < halfSize - 2; ++i) {
        if (powerDb[i] > minThresholdDb &&
            powerDb[i] > powerDb[i - 1] && powerDb[i] > powerDb[i + 1] &&
            powerDb[i] > powerDb[i - 2] && powerDb[i] > powerDb[i + 2]) {

            // Sub-bin quadratic interpolation for peak frequency
            double alpha = powerDb[i - 1];
            double beta = powerDb[i];
            double gamma = powerDb[i + 1];
            double p = 0.5 * (alpha - gamma) / (alpha - 2.0 * beta + gamma);
            double exactBin = static_cast<double>(i) + p;
            double freqHz = (exactBin * sampleRateHz_) / fftSize_;

            peaks.push_back({freqHz, beta, (sampleRateHz_ / static_cast<double>(fftSize_))});
        }
    }

    // Sort by magnitude descending
    std::sort(peaks.begin(), peaks.end(), [](const SpectralPeak& a, const SpectralPeak& b) {
        return a.magnitudeDb > b.magnitudeDb;
    });

    if (peaks.size() > maxPeaks) {
        peaks.resize(maxPeaks);
    }

    return peaks;
}

TappingPatternResult AcousticResonanceDetector::analyzeTappingPatterns() const {
    TappingPatternResult result{};
    result.isRhythmicTappingDetected = false;
    result.isSosMorseCode = false;
    result.estimatedPeriodMs = 0.0;
    result.patternRegularityScore = 0.0;
    result.detectedTaps = detectedTaps_;

    if (detectedTaps_.size() < 4) {
        return result;
    }

    // Compute inter-tap intervals (ITI) in milliseconds
    std::vector<double> intervals;
    intervals.reserve(detectedTaps_.size() - 1);
    for (size_t i = 1; i < detectedTaps_.size(); ++i) {
        double dt = static_cast<double>(detectedTaps_[i].timestampMs - detectedTaps_[i - 1].timestampMs);
        if (dt > 100.0 && dt < 3000.0) { // filter realistic human tapping range (100ms - 3s)
            intervals.push_back(dt);
        }
    }

    if (intervals.size() < 3) {
        return result;
    }

    // Mean and standard deviation of intervals
    double sum = std::accumulate(intervals.begin(), intervals.end(), 0.0);
    double mean = sum / intervals.size();

    double sqSum = 0.0;
    for (double iv : intervals) {
        sqSum += (iv - mean) * (iv - mean);
    }
    double stdDev = std::sqrt(sqSum / intervals.size());
    double cv = (mean > 1e-4) ? (stdDev / mean) : 1.0; // Coefficient of Variation

    result.estimatedPeriodMs = mean;

    // A low coefficient of variation indicates highly regular rhythmic tapping (e.g. trapped survivor)
    if (cv < 0.25) {
        result.isRhythmicTappingDetected = true;
        result.patternRegularityScore = std::clamp(1.0 - cv * 3.0, 0.0, 1.0);
    }

    // Check for SOS Morse pattern (... --- ...)
    // Expect 3 rapid intervals (~300ms), 1 longer gap (~800ms), 3 longer intervals (~600ms), 1 gap, 3 rapid intervals
    if (intervals.size() >= 8) {
        int dotCount = 0;
        int dashCount = 0;
        for (double iv : intervals) {
            if (iv < 400.0) dotCount++;
            else if (iv >= 500.0 && iv <= 1200.0) dashCount++;
        }
        if (dotCount >= 5 && dashCount >= 2) {
            result.isSosMorseCode = true;
            result.isRhythmicTappingDetected = true;
            result.patternRegularityScore = 0.95;
        }
    }

    return result;
}

std::vector<int16_t> AcousticResonanceDetector::filterAmbientNoise(
    const int16_t* pcmSamples,
    size_t sampleCount,
    size_t sampleRateHz
) {
    if (pcmSamples == nullptr || sampleCount == 0) return {};

    std::vector<int16_t> filtered(sampleCount);

    // Simple 2-pole high-pass IIR filter (cutoff 120 Hz) to eliminate wind rumble and helicopter low sub-harmonics
    double fc = 120.0;
    double rc = 1.0 / (2.0 * PI * fc);
    double dt = 1.0 / sampleRateHz;
    double alpha = rc / (rc + dt);

    double prevInput = static_cast<double>(pcmSamples[0]);
    double prevOutput = 0.0;

    for (size_t i = 0; i < sampleCount; ++i) {
        double currentInput = static_cast<double>(pcmSamples[i]);
        double output = alpha * (prevOutput + currentInput - prevInput);
        prevInput = currentInput;
        prevOutput = output;

        filtered[i] = static_cast<int16_t>(std::clamp(std::round(output), -32768.0, 32767.0));
    }

    return filtered;
}

} // namespace georescux::audio
