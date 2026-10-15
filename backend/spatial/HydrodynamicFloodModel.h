#ifndef GEORESCUX_BACKEND_HYDRODYNAMIC_FLOOD_MODEL_H
#define GEORESCUX_BACKEND_HYDRODYNAMIC_FLOOD_MODEL_H

#include <vector>
#include <cstddef>
#include <cmath>
#include <string>

namespace georescux::backend::spatial {

struct CellState {
    double waterDepth; // h (meters)
    double velocityX;  // u (m/s)
    double velocityY;  // v (m/s)
    double bedElevation; // zb (meters above sea level)
    double manningRoughness; // n (e.g., 0.035 for mountain gravel riverbed)
};

struct FloodInundationSummary {
    double peakWaterDepth;
    double peakFlowVelocity;
    double totalFloodedAreaM2;
    double arrivalTimeSec;
    bool isSubmerged;
};

/**
 * 2D Saint-Venant Shallow Water Equations (SWE) Numerical Engine.
 * Simulates flash floods, dam breaks, and glacial lake outburst floods (GLOF)
 * using a finite-volume numerical scheme with well-balanced hydrostatic reconstruction.
 */
class HydrodynamicFloodModel {
public:
    HydrodynamicFloodModel(
        size_t gridRows,
        size_t gridCols,
        double cellSpacingMeters,
        double gravity = 9.80665
    );

    /**
     * Initializes the terrain bed elevation and Manning roughness grid.
     */
    void setTerrain(
        const std::vector<double>& elevationGrid,
        double defaultManning = 0.035
    );

    /**
     * Injects water source (e.g. dam breach discharge or cloudburst runoff).
     */
    void injectInflow(
        size_t row,
        size_t col,
        double dischargeM3PerSec,
        double durationSec
    );

    /**
     * Sets instantaneous initial water depth (e.g., reservoir rupture).
     */
    void setReservoirBreach(
        size_t centerRow,
        size_t centerCol,
        size_t radiusCells,
        double initialDepthMeters
    );

    /**
     * Advances simulation by totalTimeSec using adaptive CFL time-stepping.
     */
    void stepSimulation(double totalTimeSec, double maxTimeStepSec = 0.5);

    /**
     * Gets current water depth at a given grid cell.
     */
    [[nodiscard]] double getWaterDepth(size_t row, size_t col) const noexcept;

    /**
     * Gets current flow speed sqrt(u^2 + v^2) at a cell.
     */
    [[nodiscard]] double getFlowSpeed(size_t row, size_t col) const noexcept;

    /**
     * Gets flood arrival time and peak hazard metrics at a cell.
     */
    [[nodiscard]] FloodInundationSummary getCellSummary(size_t row, size_t col) const noexcept;

    /**
     * Total number of flooded cells with depth > 0.05 meters.
     */
    [[nodiscard]] size_t getFloodedCellCount() const noexcept;

    /**
     * Export flood hazard perimeter as a binary grid mask.
     */
    [[nodiscard]] std::vector<uint8_t> generateHazardMask(double depthThresholdMeters = 0.3) const;

private:
    size_t rows_;
    size_t cols_;
    double dx_;
    double g_;
    double cfl_ = 0.45;
    double dryTolerance_ = 1e-4; // 0.1 mm

    std::vector<CellState> grid_;
    std::vector<double> peakDepth_;
    std::vector<double> arrivalTime_;
    double simulationTimeSec_ = 0.0;

    struct InflowSource {
        size_t row;
        size_t col;
        double discharge;
        double remainingTime;
    };
    std::vector<InflowSource> inflows_;

    [[nodiscard]] inline size_t index(size_t r, size_t c) const noexcept {
        return r * cols_ + c;
    }

    void computeFluxesAndSources(double dt, std::vector<CellState>& nextGrid);
    double computeStableTimeStep(double maxDt) const noexcept;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_HYDRODYNAMIC_FLOOD_MODEL_H
