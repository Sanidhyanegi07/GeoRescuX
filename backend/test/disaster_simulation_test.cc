#include <cassert>
#include <iostream>
#include <cmath>
#include <vector>

#include "spatial/WildfireRothermelModel.h"
#include "spatial/HydrodynamicFloodModel.h"
#include "spatial/AvalancheSnowpackModel.h"
#include "spatial/LandslideSusceptibilityEngine.h"
#include "spatial/SeismicWaveAttenuationModel.h"
#include "spatial/PlumeDispersionModel.h"
#include "dispatch/RescueDispatchOptimizer.h"
#include "realtime/RescueCommandBroker.h"

using namespace georescux::backend::spatial;
using namespace georescux::backend::dispatch;
using namespace georescux::backend::realtime;

void testRothermelWildfire() {
    std::cout << "[TEST] Running Rothermel Wildfire Rate of Spread tests..." << std::endl;
    double rosFlat = WildfireRothermelModel::calculateRateOfSpreadMetersPerMin(
        FuelModelType::PINE_CONIFER_FOREST, 0.08, 0.0, 0.0
    );
    assert(rosFlat > 0.0);

    // Wind and steep slope should drastically increase fire rate of spread
    double rosWindSlope = WildfireRothermelModel::calculateRateOfSpreadMetersPerMin(
        FuelModelType::PINE_CONIFER_FOREST, 0.08, 30.0, 0.55 // 30 km/h wind, 55% slope
    );
    assert(rosWindSlope > rosFlat * 1.5);

    double intensity = WildfireRothermelModel::calculateFirelineIntensityKwPerMeter(rosWindSlope, 0.45);
    assert(intensity > 0.0);

    double flameLen = WildfireRothermelModel::estimateFlameLengthMeters(intensity);
    assert(flameLen > 0.5);
    std::cout << "  -> Rothermel ROS: " << rosWindSlope << " m/min, Flame: " << flameLen << " m [PASS]" << std::endl;
}

void testHydrodynamicFlood() {
    std::cout << "[TEST] Running 2D Saint-Venant Shallow Water Equations solver..." << std::endl;
    HydrodynamicFloodModel model(20, 20, 10.0); // 20x20 grid, 10m cells

    // Sloping riverbed descending towards center
    std::vector<double> elev(400, 100.0);
    for (size_t r = 0; r < 20; ++r) {
        for (size_t c = 0; c < 20; ++c) {
            elev[r * 20 + c] = 100.0 + (19 - r) * 1.5; // slope downwards from row 0 to 19
        }
    }
    model.setTerrain(elev, 0.035);

    // Sudden reservoir rupture at top row
    model.setReservoirBreach(2, 10, 2, 5.0); // 5m water depth
    assert(model.getWaterDepth(2, 10) == 5.0);

    // Step simulation 5 seconds forward
    model.stepSimulation(5.0, 0.25);

    // Water must propagate downslope
    size_t flooded = model.getFloodedCellCount();
    assert(flooded >= 12);
    std::cout << "  -> Flood wave advanced: " << flooded << " inundated cells [PASS]" << std::endl;
}

void testAvalancheSnowpack() {
    std::cout << "[TEST] Running McClung Slab Avalanche & Voellmy runout tests..." << std::endl;
    AvalancheSnowpackModel avModel;

    std::vector<SnowLayer> layers = {
        {0.80, 220.0, -8.0, 8.5, false}, // dense wind slab
        {0.08, 140.0, -6.0, 0.6, true},  // persistent weak layer (buried depth hoar)
        {0.50, 320.0, -3.0, 12.0, false} // old hard snow
    };

    auto assessment = avModel.evaluateSlabRelease(layers, 38.0, 40.0, 45.0);
    assert(assessment.stabilityIndex < 1.2);
    assert(assessment.dangerLevel >= AvalancheDangerLevel::CONSIDERABLE);
    assert(assessment.isTerrainTrap == true);
    assert(assessment.estimatedPeakVelocityMps > 10.0);
    std::cout << "  -> Avalanche Danger: " << static_cast<int>(assessment.dangerLevel)
              << ", Peak Vel: " << assessment.estimatedPeakVelocityMps << " m/s [PASS]" << std::endl;
}

void testLandslideSusceptibility() {
    std::cout << "[TEST] Running Infinite Slope Stability & Iverson pore pressure tests..." << std::endl;
    LandslideSusceptibilityEngine engine;

    SoilRockProperties soil{
        .effectiveCohesionKPa = 12.0,
        .internalFrictionAngleDeg = 32.0,
        .saturatedUnitWeightKNm3 = 19.5,
        .hydraulicDiffusivityM2s = 2e-4,
        .soilRegolithDepthMeters = 2.0
    };

    // Dry slope
    RainfallEvent dryRain{0.0, 0.0, 0.0};
    auto dryRisk = engine.evaluateSlopeStability(35.0, soil, dryRain);
    assert(dryRisk.factorOfSafety > 1.0);

    // Heavy monsoon deluge with preceding saturated antecedent moisture
    RainfallEvent cloudburst{65.0, 12.0, 180.0}; // 65 mm/hr cloudburst
    auto stormRisk = engine.evaluateSlopeStability(38.0, soil, cloudburst, true);
    assert(stormRisk.factorOfSafety < 1.0);
    assert(stormRisk.category == SlopeStabilityCategory::ACTIVE_SLIP_FAILURE);
    assert(stormRisk.isHighwaySeveranceExpected == true);
    std::cout << "  -> Landslide FS under cloudburst: " << stormRisk.factorOfSafety << " (Severance active) [PASS]" << std::endl;
}

void testSeismicAttenuation() {
    std::cout << "[TEST] Running NGA-West GMPE & Earthquake Fragility tests..." << std::endl;
    SeismicHypocenter hypo{
        .epicenterLat = 30.50,
        .epicenterLon = 79.20,
        .focalDepthKm = 12.0,
        .momentMagnitudeMw = 7.4,
        .ruptureTimestampMs = 1600000000000ULL
    };

    // Location 25km away from epicenter
    auto motionNear = SeismicWaveAttenuationModel::evaluateGroundMotion(
        hypo, 30.65, 79.35, 450.0, StructuralFragilityClass::UNREINFORCED_STONE_MASONRY
    );
    assert(motionNear.peakGroundAccelerationG > 0.15);
    assert(motionNear.modifiedMercalliIntensity >= 7.0);
    assert(motionNear.buildingCollapseProbability > 0.20);
    assert(motionNear.estimatedPWaveLeadTimeSec > 2.0);

    std::cout << "  -> Seismic PGA: " << motionNear.peakGroundAccelerationG << " g, MMI: "
              << motionNear.modifiedMercalliIntensity << ", EEW Lead: "
              << motionNear.estimatedPWaveLeadTimeSec << " s [PASS]" << std::endl;
}

void testPlumeDispersion() {
    std::cout << "[TEST] Running Atmospheric Gaussian Plume & Briggs rise tests..." << std::endl;
    ChemicalReleaseSource src{
        .latitude = 30.00,
        .longitude = 78.00,
        .emissionRateGramsPerSec = 500.0, // 500 g/s chlorine leak
        .releaseHeightMeters = 5.0,
        .stackGasVelocityMps = 15.0,
        .stackGasTempKelvin = 330.0,
        .ambientTempKelvin = 295.0,
        .stackDiameterMeters = 1.0
    };

    double concNear = PlumeDispersionModel::calculateConcentrationMgM3(
        200.0, 0.0, 3.5, StabilityClass::F_MODERATELY_STABLE, src
    );
    assert(concNear > 5.0); // lethal concentration near source

    auto boundary = PlumeDispersionModel::generateExclusionBoundary(
        src, 45.0, 3.5, StabilityClass::F_MODERATELY_STABLE, 1.0, 3000.0
    );
    assert(!boundary.empty());
    std::cout << "  -> Plume centerline conc: " << concNear << " mg/m3, Perimeter vertices: "
              << boundary.size() << " [PASS]" << std::endl;
}

void testRescueDispatch() {
    std::cout << "[TEST] Running Multi-Depot Rescue Fleet Dispatch Optimizer..." << std::endl;
    std::vector<RescueDepot> depots = {
        {"D1", "Rishikesh NDRF Base", 30.10, 78.30, 5}
    };

    std::vector<RescueVehicle> fleet = {
        {"V1", "D1", VehicleType::ALS_AMBULANCE, 4, 2, 80.0, 45.0, 30.10, 78.30, true},
        {"V2", "D1", VehicleType::RESCUE_HELICOPTER, 6, 4, 200.0, 180.0, 30.10, 78.30, true}
    };

    std::vector<RescueVictimTarget> victims = {
        {"VIC1", 30.35, 78.60, TriageUrgency::RED_IMMEDIATE, 2, 2, 0.0, 3600.0, 600.0},
        {"VIC2", 30.20, 78.45, TriageUrgency::YELLOW_DELAYED, 1, 0, 0.0, 7200.0, 300.0}
    };

    auto sol = RescueDispatchOptimizer::optimizeDispatch(depots, fleet, victims);
    assert(sol.routes.size() == 2);
    assert(sol.totalTriageScore > 0.0);
    std::cout << "  -> Dispatch routes generated: " << sol.routes.size()
              << ", Triage priority score: " << sol.totalTriageScore << " [PASS]" << std::endl;
}

void testRescueCommandBroker() {
    std::cout << "[TEST] Running WebSocket RFC-6455 and Pub-Sub Broker tests..." << std::endl;
    RescueCommandBroker broker;

    bool receivedMessage = false;
    uint32_t cid = broker.registerClient("COMMAND_CENTER", [&](const CommandMessage& msg) {
        if (msg.topic == "incident/rudraprayag/triage") {
            receivedMessage = true;
        }
    });

    broker.subscribe(cid, "incident/+/triage");
    broker.publish("incident/rudraprayag/triage", "{\"status\": \"RED_EVACUATION\"}", 999, 3);
    assert(receivedMessage == true);

    // WebSocket Framing
    std::string testPayload = "SOS_BROADCAST_BEACON_PAYLOAD";
    auto frame = RescueCommandBroker::encodeWebSocketFrame(testPayload, WsOpcode::TEXT, false);

    std::string decodedPayload;
    WsOpcode opcode;
    size_t consumed;
    bool ok = RescueCommandBroker::decodeWebSocketFrame(frame.data(), frame.size(), decodedPayload, opcode, consumed);
    assert(ok == true);
    assert(decodedPayload == testPayload);
    assert(opcode == WsOpcode::TEXT);

    std::cout << "  -> Broker delivered message and WebSocket framing verified [PASS]" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   RUNNING GEORESCUX DISASTER PHYSICS & DISPATCH TEST     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    testRothermelWildfire();
    testHydrodynamicFlood();
    testAvalancheSnowpack();
    testLandslideSusceptibility();
    testSeismicAttenuation();
    testPlumeDispersion();
    testRescueDispatch();
    testRescueCommandBroker();

    std::cout << "==========================================================" << std::endl;
    std::cout << "   ALL ADVANCED C++ DISASTER SIMULATION TESTS PASSED!     " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
