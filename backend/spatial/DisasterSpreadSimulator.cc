#include "DisasterSpreadSimulator.h"
#include <algorithm>
#include <cmath>

namespace georescux::backend::spatial {

DisasterSpreadSimulator::DisasterSpreadSimulator(
    size_t gridRows, size_t gridCols,
    double cellResolutionMeters,
    DisasterType type
) : rows_(std::max<size_t>(3, gridRows)),
    cols_(std::max<size_t>(3, gridCols)),
    resolutionMeters_(std::max(10.0, cellResolutionMeters)),
    type_(type) {

    grid_.resize(rows_, std::vector<SimulationCell>(cols_));
    nextIntensity_.resize(rows_, std::vector<double>(cols_, 0.0));

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            grid_[r][c].elevationMeters = 500.0;
            grid_[r][c].hazardIntensity = 0.0;
            grid_[r][c].isIgnitedOrInundated = false;
        }
    }
}

void DisasterSpreadSimulator::setElevationGrid(const std::vector<std::vector<double>>& elevations) {
    for (size_t r = 0; r < rows_ && r < elevations.size(); ++r) {
        for (size_t c = 0; c < cols_ && c < elevations[r].size(); ++c) {
            grid_[r][c].elevationMeters = elevations[r][c];
        }
    }
}

void DisasterSpreadSimulator::injectEpicenter(size_t row, size_t col, double initialIntensity) {
    if (row < rows_ && col < cols_) {
        grid_[row][col].hazardIntensity = std::clamp(initialIntensity, 0.0, 1.0);
        grid_[row][col].isIgnitedOrInundated = (grid_[row][col].hazardIntensity > 0.1);
    }
}

double DisasterSpreadSimulator::getThreatAt(size_t row, size_t col) const {
    if (row < rows_ && col < cols_) {
        return grid_[row][col].hazardIntensity;
    }
    return 0.0;
}

size_t DisasterSpreadSimulator::getActiveThreatCellCount() const noexcept {
    size_t count = 0;
    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            if (grid_[r][c].isIgnitedOrInundated) {
                count++;
            }
        }
    }
    return count;
}

void DisasterSpreadSimulator::simulateFloodStep(double dt, double rainfallRateMmHour) {
    // 2D 4-connectivity gravity-driven shallow water diffusion
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            nextIntensity_[r][c] = grid_[r][c].hazardIntensity;

            // Inflow from rainfall
            if (rainfallRateMmHour > 50.0) {
                nextIntensity_[r][c] += (rainfallRateMmHour / 3600.0) * (dt / 1000.0);
            }
        }
    }

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            const double currentWater = grid_[r][c].hazardIntensity;
            if (currentWater <= 0.01) continue;

            const double currentHead = grid_[r][c].elevationMeters + currentWater * 5.0; // water height

            for (int i = 0; i < 4; ++i) {
                const int nr = static_cast<int>(r) + dr[i];
                const int nc = static_cast<int>(c) + dc[i];

                if (nr >= 0 && nr < static_cast<int>(rows_) && nc >= 0 && nc < static_cast<int>(cols_)) {
                    const double neighborHead = grid_[nr][nc].elevationMeters + grid_[nr][nc].hazardIntensity * 5.0;
                    if (currentHead > neighborHead) {
                        const double headDiff = currentHead - neighborHead;
                        // Manning flow approximation
                        const double transfer = std::min(currentWater * 0.25, (headDiff / resolutionMeters_) * dt * 0.1);
                        if (transfer > 0.0) {
                            nextIntensity_[r][c] -= transfer;
                            nextIntensity_[nr][nc] += transfer;
                        }
                    }
                }
            }
        }
    }

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            grid_[r][c].hazardIntensity = std::clamp(nextIntensity_[r][c], 0.0, 1.0);
            grid_[r][c].isIgnitedOrInundated = (grid_[r][c].hazardIntensity > 0.15);
        }
    }
}

void DisasterSpreadSimulator::simulateFireStep(double dt, double windSpeedKmh) {
    // Rothermel fire spread across 8-connectivity grid with wind alignment
    const int dr[8] = {-1, -1, -1,  0, 0,  1,  1, 1};
    const int dc[8] = {-1,  0,  1, -1, 1, -1,  0, 1};

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            nextIntensity_[r][c] = grid_[r][c].hazardIntensity;
        }
    }

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            if (!grid_[r][c].isIgnitedOrInundated) continue;

            for (int i = 0; i < 8; ++i) {
                const int nr = static_cast<int>(r) + dr[i];
                const int nc = static_cast<int>(c) + dc[i];

                if (nr >= 0 && nr < static_cast<int>(rows_) && nc >= 0 && nc < static_cast<int>(cols_)) {
                    if (!grid_[nr][nc].isIgnitedOrInundated) {
                        // Slope effect: fires travel faster uphill
                        const double slope = (grid_[nr][nc].elevationMeters - grid_[r][c].elevationMeters) / resolutionMeters_;
                        double rateOfSpread = 0.05 * (1.0 + 2.0 * std::max(0.0, slope));
                        rateOfSpread *= (1.0 + windSpeedKmh * 0.02);

                        const double ignitionDelta = rateOfSpread * dt;
                        nextIntensity_[nr][nc] += ignitionDelta;
                    }
                }
            }
        }
    }

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            grid_[r][c].hazardIntensity = std::clamp(nextIntensity_[r][c], 0.0, 1.0);
            grid_[r][c].isIgnitedOrInundated = (grid_[r][c].hazardIntensity >= 0.5);
        }
    }
}

void DisasterSpreadSimulator::step(double timeStepSeconds, double environmentalFactor) {
    if (type_ == DisasterType::FLASH_FLOOD || type_ == DisasterType::LANDSLIDE_DEBRIS_FLOW) {
        simulateFloodStep(timeStepSeconds, environmentalFactor);
    } else {
        simulateFireStep(timeStepSeconds, environmentalFactor);
    }
}

EvacuationThreatPrediction DisasterSpreadSimulator::evaluateRoadCorridor(
    const std::string& roadId,
    const std::vector<std::pair<size_t, size_t>>& roadGridCells,
    double maxSimulationHours
) {
    const double dt = 30.0; // 30-sec simulation steps
    const double totalSteps = (maxSimulationHours * 3600.0) / dt;

    double maxObservedThreat = 0.0;

    for (double s = 0; s < totalSteps; ++s) {
        step(dt, 60.0);

        for (const auto& [r, c] : roadGridCells) {
            if (r < rows_ && c < cols_) {
                const double threat = grid_[r][c].hazardIntensity;
                maxObservedThreat = std::max(maxObservedThreat, threat);
                if (threat >= 0.6) {
                    return EvacuationThreatPrediction{
                        .roadSegmentId = roadId,
                        .estimatedTimeUntilCutoffMinutes = (s * dt) / 60.0,
                        .peakThreatIntensity = threat,
                        .willBeCutOff = true
                    };
                }
            }
        }
    }

    return EvacuationThreatPrediction{
        .roadSegmentId = roadId,
        .estimatedTimeUntilCutoffMinutes = maxSimulationHours * 60.0,
        .peakThreatIntensity = maxObservedThreat,
        .willBeCutOff = false
    };
}

} // namespace georescux::backend::spatial
