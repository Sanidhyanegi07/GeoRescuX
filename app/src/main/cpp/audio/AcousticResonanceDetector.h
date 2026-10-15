#ifndef GEORESCUX_ACOUSTIC_RESONANCE_DETECTOR_H
#define GEORESCUX_ACOUSTIC_RESONANCE_DETECTOR_H

#include <vector>
#include <complex>
#include <cstdint>
#include <cstddef>

namespace georescux::audio {

struct SpectralPeak {
    double frequencyHz;
    double magnitudeDb;
    double bandwidthHz;
};

struct TapEvent {
    uint64_t timestampMs;
    double amplitude;
    double confidence;
};

struct TappingPatternResult {
    bool isRhythmicTappingDetected;
    bool isSosMorseCode; // ... --- ...
    double estimatedPeriodMs;
    double patternRegularityScore; // 0.0 to 1.0
    std::vector<TapEvent> detectedTaps;
};

/**
 * Acoustic Resonance and Rubble Tapping Detector.
 * Analyzes microphone PCM audio in disaster search-and-rescue operations
 * to detect faint rhythmic tapping, structural resonance, and SOS Morse code
 * through concrete rubble, metal pipes, and debris.
 */
class AcousticResonanceDetector {
public:
    AcousticResonanceDetector(size_t sampleRateHz = 16000, size_t fftSize = 1024);

    /**
     * Ingests 16-bit PCM audio samples and updates spectral and temporal history.
     */
    void processPcmAudio(const int16_t* pcmSamples, size_t sampleCount, uint64_t startTimestampMs);

    /**
     * Evaluates whether rhythmic tapping or SOS Morse code is present in current audio buffer.
     */
    [[nodiscard]] TappingPatternResult analyzeTappingPatterns() const;

    /**
     * Computes Fast Fourier Transform (FFT) spectrum and returns dominant resonant frequencies.
     */
    [[nodiscard]] std::vector<SpectralPeak> getResonantPeaks(double minThresholdDb = -45.0, size_t maxPeaks = 5) const;

    /**
     * Applies spectral subtraction noise filtering to remove constant ambient noise (wind, helicopter rotors).
     */
    static std::vector<int16_t> filterAmbientNoise(
        const int16_t* pcmSamples,
        size_t sampleCount,
        size_t sampleRateHz = 16000
    );

    /**
     * Cooley-Tukey radix-2 Decimation-In-Time (DIT) Fast Fourier Transform.
     */
    static void computeFft(std::vector<std::complex<double>>& buffer, bool inverse = false);

private:
    size_t sampleRateHz_;
    size_t fftSize_;
    std::vector<double> hanningWindow_;
    std::vector<double> audioBuffer_;
    std::vector<TapEvent> detectedTaps_;

    void initializeWindow();
    static size_t bitReverse(size_t n, size_t bits) noexcept;
};

} // namespace georescux::audio

#endif // GEORESCUX_ACOUSTIC_RESONANCE_DETECTOR_H
