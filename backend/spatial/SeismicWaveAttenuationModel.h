#ifndef GEORESCUX_BACKEND_SEISMIC_WAVE_ATTENUATION_MODEL_H
#define GEORESCUX_BACKEND_SEISMIC_WAVE_ATTENUATION_MODEL_H

#include <vector>
#include <string>
#include <cmath>

namespace georescux::backend::spatial {

enum class StructuralFragilityClass {
    UNREINFORCED_STONE_MASONRY, // Traditional Himalayan village stone-in-mud
    REINFORCED_CONCRETE_FRAME,  // Urban RCC multistory buildings
    TIMBER_DHAJJI_DEWARI,       // Traditional ductile timber-laced earthquake architecture
    ENGINEERED_STEEL_STRUCTURE  // Disaster bridges, modern base stations
};

struct SeismicHypocenter {
    double epicenterLat;
    double epicenterLon;
    double focalDepthKm;
    double momentMagnitudeMw;
    uint64_t ruptureTimestampMs;
};

struct GroundMotionParameters {
    double peakGroundAccelerationG; // PGA in g (fraction of standard gravity)
    double peakGroundVelocityCmSec;  // PGV in cm/s
    double spectralAcceleration1s;   // Sa at 1.0s period in g
    double modifiedMercalliIntensity;// MMI scale (I to XII)
    double buildingCollapseProbability;
    double estimatedPWaveLeadTimeSec; // Earthquake Early Warning (EEW) warning window
};

/**
 * NGA-West Ground Motion Prediction Equations (GMPE) and Seismic Fragility Engine.
 * Evaluates Himalayan Main Boundary/Central Thrust (MBT/MCT) earthquake wave propagation,
 * site amplification (Vs30), and structural collapse probability for urban and rural Uttarakhand.
 */
class SeismicWaveAttenuationModel {
public:
    SeismicWaveAttenuationModel() = default;

    /**
     * Calculates peak ground motion and MMI at a target location.
     * @param hypo Hypocenter parameters
     * @param targetLat Latitude of recipient
     * @param targetLon Longitude of recipient
     * @param vs30Mps Average shear-wave velocity in top 30m of soil (e.g. 760 m/s for rock, 300 m/s for alluvium)
     * @param buildingType Local construction typology
     */
    [[nodiscard]] static GroundMotionParameters evaluateGroundMotion(
        const SeismicHypocenter& hypo,
        double targetLat,
        double targetLon,
        double vs30Mps = 760.0,
        StructuralFragilityClass buildingType = StructuralFragilityClass::UNREINFORCED_STONE_MASONRY
    ) noexcept;

    /**
     * Converts Peak Ground Acceleration (PGA) to Modified Mercalli Intensity (MMI)
     * using Wald et al. (1999) USGS Shakemap formulations.
     */
    [[nodiscard]] static double pgaToModifiedMercalli(double pgaG) noexcept;

    /**
     * Evaluates structural collapse probability given MMI and building fragility class.
     */
    [[nodiscard]] static double calculateStructuralCollapseProbability(
        double mmi,
        StructuralFragilityClass type
    ) noexcept;

    /**
     * Computes distance in kilometers between hypocenter and target site.
     */
    [[nodiscard]] static double ruptureDistanceKm(
        double epiLat, double epiLon, double focalDepthKm,
        double targetLat, double targetLon
    ) noexcept;
};

} // namespace georescux::backend::spatial

#endif // GEORESCUX_BACKEND_SEISMIC_WAVE_ATTENUATION_MODEL_H
