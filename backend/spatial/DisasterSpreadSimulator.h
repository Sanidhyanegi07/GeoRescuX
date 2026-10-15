#ifndef GEORESCUX_BACKEND_DISASTER_SPREAD_SIMULATOR_H
#define GEORESCUX_BACKEND_DISASTER_SPREAD_SIMULATOR_H

#include <vector>
#include <string>
#include <cstdint>

namespace georescux::backend::spatial {

enum class DisasterType {
    FLASH_FLOOD,
    WILDFIRE,
    LANDSLIDE_DEBRIS_FLOW
};

struct SimulationCell {
    double latitude;
    double longitude;
    double elevationMeters;
    double hazardIntensity{0.0}; // 0.0 = safe, 1.0 = fully inundated/consumed
    bool isIgnitedOrInundated{false};
};

struct EvacuationThreatPrediction {
    std::string roadSegmentId;
    double estimatedTimeUntilCutoffMinutes;
    double peakThreatIntensity;
    bool willBeCutOff;
};

/**
 * 2D Cellular Automata simulation engine for disaster front propagation.
 * Models fluid dynamic shallow-water equations / Rothermel fire spread across
 * DEM (Digital Elevation Model) terrain grids to predict road cutoffs hours in advance.
 */
class DisasterSpreadSimulator {
public:
    DisasterSpreadSimulator(
        size_t gridRows, size_t gridCols,
        double cellResolutionMeters = 100.0,
        DisasterType type = DisasterType::FLASH_FLOOD
    );

    /**
     * Initializes grid with elevation matrix.
     */
    void setElevationGrid(const std::vector<std::vector<double>>& elevations);

    /**
     * Injects an initial disaster breach (e.g. dam failure or landslide epicenter).
     */
    void injectEpicenter(size_t row, size_t col, double initialIntensity = 1.0);

    /**
     * Advances the simulation by timeStepSeconds.
     */
    void step(double timeStepSeconds, double environmentalFactor = 1.0);

    /**
     * Checks if a target coordinate is inundated/threatened.
     */
    [[nodiscard]] double getThreatAt(size_t row, size_t col) const;

    [[nodiscard]] size_t getActiveThreatCellCount() const noexcept;

    /**
     * Evaluates a road evacuation corridor against the forecasted disaster wave.
     */
    EvacuationThreatPrediction evaluateRoadCorridor(
        const std::string& roadId,
        const std::vector<std::pair<size_t, size_t>>& roadGridCells,
        double maxSimulationHours = 6.0
    );

private:
    size_t rows_;
    size_t cols_;
    double resolutionMeters_;
    DisasterType type_;

    std::vector<std::vector<SimulationCell>> grid_;
    std::vector<std::vector<double>> nextIntensity_;

    void simulateFloodStep(double dt, double rainfallRateMmHour);
    void simulateFireStep(double dt, double windSpeedKmh);
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_DISASTER_SPREAD_SIMULATOR_H
