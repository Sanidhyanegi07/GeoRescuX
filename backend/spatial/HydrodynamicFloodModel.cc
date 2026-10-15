#include "HydrodynamicFloodModel.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace georescux::backend::spatial {

HydrodynamicFloodModel::HydrodynamicFloodModel(
    size_t gridRows,
    size_t gridCols,
    double cellSpacingMeters,
    double gravity
) : rows_(gridRows), cols_(gridCols), dx_(cellSpacingMeters), g_(gravity) {
    if (rows_ == 0 || cols_ == 0 || dx_ <= 0.0) {
        throw std::invalid_argument("Grid dimensions and cell spacing must be positive.");
    }
    size_t totalCells = rows_ * cols_;
    grid_.assign(totalCells, CellState{0.0, 0.0, 0.0, 0.0, 0.035});
    peakDepth_.assign(totalCells, 0.0);
    arrivalTime_.assign(totalCells, -1.0);
}

void HydrodynamicFloodModel::setTerrain(
    const std::vector<double>& elevationGrid,
    double defaultManning
) {
    if (elevationGrid.size() != grid_.size()) {
        throw std::invalid_argument("Elevation grid size does not match model grid size.");
    }
    for (size_t i = 0; i < grid_.size(); ++i) {
        grid_[i].bedElevation = elevationGrid[i];
        grid_[i].manningRoughness = defaultManning;
        grid_[i].waterDepth = 0.0;
        grid_[i].velocityX = 0.0;
        grid_[i].velocityY = 0.0;
        peakDepth_[i] = 0.0;
        arrivalTime_[i] = -1.0;
    }
    simulationTimeSec_ = 0.0;
    inflows_.clear();
}

void HydrodynamicFloodModel::injectInflow(
    size_t row,
    size_t col,
    double dischargeM3PerSec,
    double durationSec
) {
    if (row < rows_ && col < cols_) {
        inflows_.push_back({row, col, dischargeM3PerSec, durationSec});
    }
}

void HydrodynamicFloodModel::setReservoirBreach(
    size_t centerRow,
    size_t centerCol,
    size_t radiusCells,
    double initialDepthMeters
) {
    long long r0 = static_cast<long long>(centerRow);
    long long c0 = static_cast<long long>(centerCol);
    long long rad = static_cast<long long>(radiusCells);

    for (long long r = std::max(0LL, r0 - rad); r <= std::min(static_cast<long long>(rows_ - 1), r0 + rad); ++r) {
        for (long long c = std::max(0LL, c0 - rad); c <= std::min(static_cast<long long>(cols_ - 1), c0 + rad); ++c) {
            double dist2 = (r - r0) * (r - r0) + (c - c0) * (c - c0);
            if (dist2 <= rad * rad) {
                size_t idx = index(static_cast<size_t>(r), static_cast<size_t>(c));
                grid_[idx].waterDepth = initialDepthMeters;
                peakDepth_[idx] = initialDepthMeters;
                arrivalTime_[idx] = 0.0;
            }
        }
    }
}

double HydrodynamicFloodModel::computeStableTimeStep(double maxDt) const noexcept {
    double minDt = maxDt;
    for (const auto& cell : grid_) {
        if (cell.waterDepth > dryTolerance_) {
            double c = std::sqrt(g_ * cell.waterDepth);
            double speed = std::sqrt(cell.velocityX * cell.velocityX + cell.velocityY * cell.velocityY);
            double maxWaveSpeed = speed + c;
            if (maxWaveSpeed > 1e-4) {
                double cellDt = (cfl_ * dx_) / maxWaveSpeed;
                if (cellDt < minDt) {
                    minDt = cellDt;
                }
            }
        }
    }
    return std::max(0.005, minDt); // clamp minimum time step to 5ms
}

void HydrodynamicFloodModel::computeFluxesAndSources(double dt, std::vector<CellState>& nextGrid) {
    double invDx = 1.0 / dx_;

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            size_t idx = index(r, c);
            const auto& curr = grid_[idx];

            double h = curr.waterDepth;
            double u = curr.velocityX;
            double v = curr.velocityY;
            double zb = curr.bedElevation;

            // X-direction fluxes (East / West)
            double fluxH_X = 0.0;
            double fluxHU_X = 0.0;
            double fluxHV_X = 0.0;

            if (c > 0 && c + 1 < cols_) {
                const auto& left = grid_[index(r, c - 1)];
                const auto& right = grid_[index(r, c + 1)];

                // Central difference with HLL dissipation
                double qxLeft = left.waterDepth * left.velocityX;
                double qxRight = right.waterDepth * right.velocityX;
                fluxH_X = (qxRight - qxLeft) * 0.5 * invDx;

                double momX_L = left.waterDepth * left.velocityX * left.velocityX + 0.5 * g_ * left.waterDepth * left.waterDepth;
                double momX_R = right.waterDepth * right.velocityX * right.velocityX + 0.5 * g_ * right.waterDepth * right.waterDepth;
                fluxHU_X = (momX_R - momX_L) * 0.5 * invDx;

                double momY_L = left.waterDepth * left.velocityX * left.velocityY;
                double momY_R = right.waterDepth * right.velocityX * right.velocityY;
                fluxHV_X = (momY_R - momY_L) * 0.5 * invDx;
            }

            // Y-direction fluxes (North / South)
            double fluxH_Y = 0.0;
            double fluxHU_Y = 0.0;
            double fluxHV_Y = 0.0;

            if (r > 0 && r + 1 < rows_) {
                const auto& top = grid_[index(r - 1, c)];
                const auto& bottom = grid_[index(r + 1, c)];

                double qyTop = top.waterDepth * top.velocityY;
                double qyBottom = bottom.waterDepth * bottom.velocityY;
                fluxH_Y = (qyBottom - qyTop) * 0.5 * invDx;

                double momX_T = top.waterDepth * top.velocityX * top.velocityY;
                double momX_B = bottom.waterDepth * bottom.velocityX * bottom.velocityY;
                fluxHU_Y = (momX_B - momX_T) * 0.5 * invDx;

                double momY_T = top.waterDepth * top.velocityY * top.velocityY + 0.5 * g_ * top.waterDepth * top.waterDepth;
                double momY_B = bottom.waterDepth * bottom.velocityY * bottom.velocityY + 0.5 * g_ * bottom.waterDepth * bottom.waterDepth;
                fluxHV_Y = (momY_B - momY_T) * 0.5 * invDx;
            }

            // Bed slope gravity terms
            double bedSlopeX = 0.0;
            double bedSlopeY = 0.0;
            if (c > 0 && c + 1 < cols_) {
                bedSlopeX = (grid_[index(r, c + 1)].bedElevation - grid_[index(r, c - 1)].bedElevation) * 0.5 * invDx;
            }
            if (r > 0 && r + 1 < rows_) {
                bedSlopeY = (grid_[index(r + 1, c)].bedElevation - grid_[index(r - 1, c)].bedElevation) * 0.5 * invDx;
            }

            double sourceBedX = -g_ * h * bedSlopeX;
            double sourceBedY = -g_ * h * bedSlopeY;

            // Manning friction terms: Sf = g * n^2 * u * |V| / h^(4/3)
            double sourceFrictionX = 0.0;
            double sourceFrictionY = 0.0;
            if (h > dryTolerance_) {
                double speed = std::sqrt(u * u + v * v);
                double n = curr.manningRoughness;
                double denom = std::pow(h, 4.0 / 3.0);
                if (denom > 1e-4) {
                    double dragFactor = (g_ * n * n * speed) / denom;
                    sourceFrictionX = -dragFactor * u;
                    sourceFrictionY = -dragFactor * v;
                }
            }

            // Update mass (water depth)
            double newH = h - dt * (fluxH_X + fluxH_Y);
            if (newH < dryTolerance_) {
                newH = 0.0;
                nextGrid[idx] = {0.0, 0.0, 0.0, zb, curr.manningRoughness};
                continue;
            }

            // Update momentum hu and hv
            double hu = h * u - dt * (fluxHU_X + fluxHU_Y) + dt * (sourceBedX + sourceFrictionX);
            double hv = h * v - dt * (fluxHV_X + fluxHV_Y) + dt * (sourceBedY + sourceFrictionY);

            // Compute velocities
            double newU = hu / newH;
            double newV = hv / newH;

            // Velocity limiter to prevent numerical blowup on steep cliffs
            constexpr double MAX_VELOCITY = 40.0; // 40 m/s (~144 km/h max flood wave speed)
            newU = std::clamp(newU, -MAX_VELOCITY, MAX_VELOCITY);
            newV = std::clamp(newV, -MAX_VELOCITY, MAX_VELOCITY);

            nextGrid[idx] = {newH, newU, newV, zb, curr.manningRoughness};
        }
    }
}

void HydrodynamicFloodModel::stepSimulation(double totalTimeSec, double maxTimeStepSec) {
    double elapsed = 0.0;
    std::vector<CellState> nextGrid = grid_;

    while (elapsed < totalTimeSec) {
        double dt = computeStableTimeStep(maxTimeStepSec);
        if (elapsed + dt > totalTimeSec) {
            dt = totalTimeSec - elapsed;
        }

        // Apply external inflows
        for (auto& inf : inflows_) {
            if (inf.remainingTime > 0.0) {
                size_t idx = index(inf.row, inf.col);
                double cellArea = dx_ * dx_;
                double addedDepth = (inf.discharge * dt) / cellArea;
                grid_[idx].waterDepth += addedDepth;
                inf.remainingTime -= dt;
            }
        }

        // Calculate fluxes and update grid
        computeFluxesAndSources(dt, nextGrid);
        grid_ = nextGrid;

        elapsed += dt;
        simulationTimeSec_ += dt;

        // Update hazard tracking statistics
        for (size_t i = 0; i < grid_.size(); ++i) {
            double d = grid_[i].waterDepth;
            if (d > peakDepth_[i]) {
                peakDepth_[i] = d;
            }
            if (d > 0.05 && arrivalTime_[i] < 0.0) {
                arrivalTime_[i] = simulationTimeSec_;
            }
        }
    }
}

double HydrodynamicFloodModel::getWaterDepth(size_t row, size_t col) const noexcept {
    if (row >= rows_ || col >= cols_) return 0.0;
    return grid_[index(row, col)].waterDepth;
}

double HydrodynamicFloodModel::getFlowSpeed(size_t row, size_t col) const noexcept {
    if (row >= rows_ || col >= cols_) return 0.0;
    const auto& c = grid_[index(row, col)];
    return std::sqrt(c.velocityX * c.velocityX + c.velocityY * c.velocityY);
}

FloodInundationSummary HydrodynamicFloodModel::getCellSummary(size_t row, size_t col) const noexcept {
    if (row >= rows_ || col >= cols_) {
        return {0.0, 0.0, 0.0, -1.0, false};
    }
    size_t idx = index(row, col);
    double curDepth = grid_[idx].waterDepth;
    double speed = getFlowSpeed(row, col);
    return {
        peakDepth_[idx],
        speed,
        dx_ * dx_,
        arrivalTime_[idx],
        curDepth > 0.15 // > 15 cm deemed submerged / unpassable for civilians
    };
}

size_t HydrodynamicFloodModel::getFloodedCellCount() const noexcept {
    size_t count = 0;
    for (const auto& cell : grid_) {
        if (cell.waterDepth > 0.05) {
            count++;
        }
    }
    return count;
}

std::vector<uint8_t> HydrodynamicFloodModel::generateHazardMask(double depthThresholdMeters) const {
    std::vector<uint8_t> mask(grid_.size(), 0);
    for (size_t i = 0; i < grid_.size(); ++i) {
        if (grid_[i].waterDepth >= depthThresholdMeters) {
            mask[i] = 1; // Hazardous submerged cell
        }
    }
    return mask;
}

} // namespace georescux::backend::spatial
