#include "PrototypeBackend.h"
#include "GeodesicMath.h"
#include "RouteGraph.h"
#include "AStarPathfinder.h"
#include "MeshPacketCodec.h"
#include "PacketAuthenticator.h"
#include "TriageEngine.h"
#include "CasualtySurvivalProbability.h"
#include "KDTree.h"
#include "KalmanFusedLocator.h"

#include <QDebug>
#include <QDateTime>
#include <sstream>
#include <iomanip>

using namespace georescux::spatial;
using namespace georescux::routing;
using namespace georescux::mesh;
using namespace georescux::crypto;
using namespace georescux::triage;
using namespace georescux::analytics;
using namespace georescux::telemetry;

PrototypeBackend::PrototypeBackend(QObject *parent)
    : QObject(parent),
      m_statusText(QStringLiteral("Ready — Native C++20 Core Loaded")),
      m_routeSummary(QStringLiteral("No route calculated. Run A* routing benchmark below.")),
      m_meshSummary(QStringLiteral("No mesh packet processed yet.")),
      m_triageSummary(QStringLiteral("No casualty assessed yet.")),
      m_spatialSummary(QStringLiteral("No spatial query executed yet.")),
      m_kalmanLocator(std::make_unique<KalmanFusedLocator>(30.3165, 78.0322, 650.0))
{
    appendLog(QStringLiteral("Native C++20 GeoRescuX Engine fully initialized (Routing, Mesh, Triage, Telemetry)."));
}

PrototypeBackend::~PrototypeBackend() = default;

QString PrototypeBackend::statusText() const noexcept {
    return m_statusText;
}

int PrototypeBackend::triggerCount() const noexcept {
    return m_triggerCount;
}

double PrototypeBackend::lastComputedDistance() const noexcept {
    return m_lastComputedDistance;
}

QString PrototypeBackend::routeSummary() const noexcept {
    return m_routeSummary;
}

QString PrototypeBackend::meshSummary() const noexcept {
    return m_meshSummary;
}

QString PrototypeBackend::triageSummary() const noexcept {
    return m_triageSummary;
}

QString PrototypeBackend::spatialSummary() const noexcept {
    return m_spatialSummary;
}

// -----------------------------------------------------------------------------
// Subsystem 1: Geodesic Benchmark
// -----------------------------------------------------------------------------
void PrototypeBackend::runGeodesicBenchmark() {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    constexpr double lat1 = 30.3165; // Dehradun
    constexpr double lon1 = 78.0322;
    constexpr double lat2 = 30.0869; // Rishikesh
    constexpr double lon2 = 78.2676;

    const double haversineMeters = GeodesicMath::haversineDistanceMeters(lat1, lon1, lat2, lon2);
    const double vincentyMeters = GeodesicMath::vincentyDistanceMeters(lat1, lon1, lat2, lon2);
    const double bearing = GeodesicMath::initialBearingDegrees(lat1, lon1, lat2, lon2);

    m_lastComputedDistance = vincentyMeters;
    const double diffMeters = std::abs(haversineMeters - vincentyMeters);

    const QString details = QStringLiteral("Vincenty: %1 m | Haversine: %2 m | Delta: %3 m | Bearing: %4°")
        .arg(vincentyMeters, 0, 'f', 2)
        .arg(haversineMeters, 0, 'f', 2)
        .arg(diffMeters, 0, 'f', 3)
        .arg(bearing, 0, 'f', 1);

    m_statusText = QStringLiteral("Geodesic #%1: %2 m").arg(m_triggerCount).arg(vincentyMeters, 0, 'f', 1);
    emit statusChanged(m_statusText);
    emit calculationFinished(vincentyMeters, details);

    appendLog(QStringLiteral("[Geodesic] %1").arg(details));
}

// -----------------------------------------------------------------------------
// Subsystem 1 (Advanced): A* Evacuation Route Graph Search
// -----------------------------------------------------------------------------
void PrototypeBackend::runAStarRoutingBenchmark(bool avoidHazards) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    // Build real evacuation road graph for Uttarakhand corridor
    std::vector<RouteNode> nodes = {
        RouteNode{"DED_ISBT", 30.2858, 78.0098, false},
        RouteNode{"DED_CLOCK_TWR", 30.3255, 78.0436, false},
        RouteNode{"MALSI_SHELTER", 30.3789, 78.0772, true},
        RouteNode{"JOLLY_GRANT_HOSP", 30.1878, 78.1884, true},
        RouteNode{"RISHIKESH_CAMP", 30.1033, 78.2947, true},
        RouteNode{"HARIDWAR_HQ", 29.9457, 78.1642, true}
    };

    std::vector<RouteEdge> edges = {
        RouteEdge{"DED_ISBT", "DED_CLOCK_TWR", 5800.0, false},
        RouteEdge{"DED_CLOCK_TWR", "MALSI_SHELTER", 7400.0, false},
        RouteEdge{"DED_CLOCK_TWR", "JOLLY_GRANT_HOSP", 24200.0, false},
        RouteEdge{"DED_ISBT", "JOLLY_GRANT_HOSP", 27500.0, false},
        RouteEdge{"JOLLY_GRANT_HOSP", "RISHIKESH_CAMP", 16800.0, false},
        RouteEdge{"JOLLY_GRANT_HOSP", "HARIDWAR_HQ", 31000.0, false},
        RouteEdge{"RISHIKESH_CAMP", "HARIDWAR_HQ", 23500.0, false}
    };

    std::vector<RoadHazard> hazards;
    if (avoidHazards) {
        // Active flash-flood hazard on main Dehradun -> Jolly Grant direct corridor
        hazards.push_back(RoadHazard{"HAZ_01", "DED_CLOCK_TWR", "JOLLY_GRANT_HOSP", 50000.0});
    }

    RouteGraph graph(std::move(nodes), std::move(edges), std::move(hazards), 1);
    auto routeOpt = AStarPathfinder::findRoute(graph, "DED_ISBT", "RISHIKESH_CAMP");

    if (routeOpt.has_value() && !routeOpt->empty()) {
        const auto& route = *routeOpt;
        QString pathStr;
        for (size_t i = 0; i < route.nodeIds.size(); ++i) {
            if (i > 0) pathStr += QStringLiteral(" ➔ ");
            pathStr += QString::fromStdString(route.nodeIds[i]);
        }

        const int minutes = static_cast<int>(route.estimatedDurationSeconds / 60);
        m_routeSummary = QStringLiteral("Path: %1 | Distance: %2 km | Cost: %3 km | Est: %4 min")
            .arg(pathStr)
            .arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2)
            .arg(route.totalCostMeters / 1000.0, 0, 'f', 2)
            .arg(minutes);

        QString warningsStr = route.hazardWarnings.empty() ? QStringLiteral("None (Clear Road)")
            : QStringLiteral("%1 hazard(s) bypassed").arg(route.hazardWarnings.size());

        emit routeSummaryChanged(m_routeSummary);
        emit routeCalculated(pathStr, route.totalDistanceMeters, route.totalCostMeters, static_cast<int>(route.estimatedDurationSeconds), warningsStr);

        appendLog(QStringLiteral("[A* Routing] Calculated: %1 (%2 km)").arg(pathStr).arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2));
    } else {
        m_routeSummary = QStringLiteral("No viable route found between origin and destination.");
        emit routeSummaryChanged(m_routeSummary);
        appendLog(QStringLiteral("[A* Routing] No viable route found."));
    }
}

// -----------------------------------------------------------------------------
// Subsystem 2: Mesh Packet Codec & Cryptographic Authenticator
// -----------------------------------------------------------------------------
void PrototypeBackend::runMeshPacketBenchmark(int urgency, int battery, int peopleCount) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    const std::string senderId = "NODE_RESCUE_07";
    const double lat = 30.3165;
    const double lon = 78.0322;
    const double alt = 645.0;
    const uint32_t nowSec = static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());

    // 1. Encode bit-packed binary SOS packet
    auto packetBytes = MeshPacketCodec::encodeSosPacket(
        static_cast<uint8_t>(MeshPriority::CRITICAL),
        15, // TTL
        static_cast<uint16_t>(m_triggerCount),
        senderId,
        lat, lon, alt,
        static_cast<uint8_t>(battery),
        static_cast<uint8_t>(urgency),
        static_cast<uint16_t>(peopleCount),
        nowSec
    );

    // 2. Convert to Hex representation
    std::stringstream hexStream;
    for (uint8_t b : packetBytes) {
        hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    const std::string hexStr = hexStream.str();

    // 3. Decode packet and verify CRC-16
    auto decodedOpt = MeshPacketCodec::decodePacket(packetBytes.data(), packetBytes.size());
    const bool crcValid = decodedOpt.has_value();

    // 4. Authenticate payload with HMAC-SHA256
    const std::string presharedKey = "GEORESCUX_HMAC_SECRET_KEY_2026";
    auto hmacDigest = PacketAuthenticator::hmacSha256(
        reinterpret_cast<const uint8_t*>(presharedKey.data()), presharedKey.size(),
        packetBytes.data(), packetBytes.size()
    );
    const std::string hmacHex = PacketAuthenticator::toHexString(hmacDigest);

    QString decodedSummary;
    if (decodedOpt) {
        decodedSummary = QStringLiteral("Seq: %1 | Urgency: %2 | People: %3 | Batt: %4% | GPS: [%5, %6]")
            .arg(decodedOpt->sequenceNumber)
            .arg(decodedOpt->medicalUrgency)
            .arg(decodedOpt->peopleCount)
            .arg(decodedOpt->batteryPercent)
            .arg(decodedOpt->latitude, 0, 'f', 4)
            .arg(decodedOpt->longitude, 0, 'f', 4);
    } else {
        decodedSummary = QStringLiteral("CRC validation failed!");
    }

    m_meshSummary = QStringLiteral("Size: %1 bytes | CRC-16: %2 | HMAC: %3...")
        .arg(packetBytes.size())
        .arg(crcValid ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
        .arg(QString::fromStdString(hmacHex).left(16));

    emit meshSummaryChanged(m_meshSummary);
    emit meshPacketProcessed(QString::fromStdString(hexStr), static_cast<int>(packetBytes.size()), crcValid, QString::fromStdString(hmacHex), decodedSummary);

    appendLog(QStringLiteral("[Mesh Codec] Encoded %1 bytes, CRC: %2, HMAC: %3...")
        .arg(packetBytes.size())
        .arg(crcValid ? QStringLiteral("VALID") : QStringLiteral("INVALID"))
        .arg(QString::fromStdString(hmacHex).left(12)));
}

// -----------------------------------------------------------------------------
// Subsystem 3: MCI Disaster Triage & Survival Analytics
// -----------------------------------------------------------------------------
void PrototypeBackend::runTriageAssessment(
    bool canWalk,
    bool breathing,
    int respRate,
    bool radialPulse,
    bool hemorrhage,
    bool trapped
) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    VictimAssessment assessment;
    assessment.canWalk = canWalk;
    assessment.breathingSpontaneously = breathing;
    assessment.breathingAfterAirwayPositioned = breathing;
    assessment.respirationRatePerMinute = static_cast<uint8_t>(respRate);
    assessment.capillaryRefillSeconds = radialPulse ? 1.5 : 3.5;
    assessment.radialPulsePresent = radialPulse;
    assessment.followsSimpleCommands = canWalk || radialPulse;
    assessment.hasUncontrolledHemorrhage = hemorrhage;
    assessment.isTrapped = trapped;

    // Run START decision tree
    auto result = TriageEngine::evaluateStart(assessment);
    const uint8_t urgencyScore = TriageEngine::computeContinuousUrgencyScore(
        result.category, hemorrhage, trapped, static_cast<uint8_t>(respRate), radialPulse
    );

    // Run hypothermia & casualty survival probability model
    CasualtyProfile profile;
    profile.ageYears = 34;
    profile.trappedDurationHours = trapped ? 3.5 : 0.5;
    profile.ambientTemperatureCelsius = 4.0; // Cold mountain night
    profile.isWetOrSubmerged = trapped;
    profile.hasSevereBleeding = hemorrhage;
    profile.hasCrushSyndromeRisk = trapped;
    profile.hasConfirmedAirPocket = true;
    profile.clothingInsulationClo = 1.0;

    auto survivalEst = CasualtySurvivalProbability::computeSurvivalProbability(
        profile,
        trapped ? EntrapmentEnvironment::CONCRETE_RUBBLE_COLLAPSE : EntrapmentEnvironment::OPEN_TERRAIN_EXPOSURE
    );

    const QString catStr = QString::fromUtf8(TriageEngine::categoryToString(result.category));
    const QString reasonStr = QString::fromStdString(result.primaryReason);
    const QString actionStr = QString::fromStdString(result.recommendedIntervention);

    m_triageSummary = QStringLiteral("Category: %1 | Urgency: %2/100 | Survival: %3%")
        .arg(catStr)
        .arg(urgencyScore)
        .arg(survivalEst.overallSurvivalProbability * 100.0, 0, 'f', 1);

    emit triageSummaryChanged(m_triageSummary);
    emit triageEvaluated(catStr, urgencyScore, reasonStr, actionStr, survivalEst.overallSurvivalProbability);

    appendLog(QStringLiteral("[MCI Triage] Tag: %1 (Urgency %2) | %3 | Survival: %4%")
        .arg(catStr).arg(urgencyScore).arg(reasonStr).arg(survivalEst.overallSurvivalProbability * 100.0, 0, 'f', 1));
}

// -----------------------------------------------------------------------------
// Subsystem 4: Spatial KD-Tree Shelter Lookup
// -----------------------------------------------------------------------------
void PrototypeBackend::runSpatialShelterQuery(double lat, double lon) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    KDTree tree;
    std::vector<SpatialPoint> shelters = {
        SpatialPoint{"Dehradun Parade Ground Shelter", 30.3255, 78.0436, 640.0, true},
        SpatialPoint{"Malsi Emergency Staging Depot", 30.3789, 78.0772, 780.0, true},
        SpatialPoint{"Jolly Grant Trauma Safe Haven", 30.1878, 78.1884, 550.0, true},
        SpatialPoint{"Rishikesh Ganga Evac Camp", 30.1033, 78.2947, 372.0, true},
        SpatialPoint{"Haridwar Disaster Relief Base", 29.9457, 78.1642, 314.0, true}
    };
    tree.build(std::move(shelters));

    auto nearestOpt = tree.findNearest(lat, lon, true);
    if (nearestOpt) {
        const double distMeters = GeodesicMath::vincentyDistanceMeters(
            lat, lon, nearestOpt->latitude, nearestOpt->longitude
        );

        m_spatialSummary = QStringLiteral("Nearest Safe Haven: %1 (%2 km away)")
            .arg(QString::fromStdString(nearestOpt->id))
            .arg(distMeters / 1000.0, 0, 'f', 2);

        emit spatialSummaryChanged(m_spatialSummary);
        emit shelterFound(QString::fromStdString(nearestOpt->id), nearestOpt->latitude, nearestOpt->longitude, distMeters);

        appendLog(QStringLiteral("[KD-Tree] Nearest Haven: %1 (%2 km)")
            .arg(QString::fromStdString(nearestOpt->id)).arg(distMeters / 1000.0, 0, 'f', 2));
    }
}

// -----------------------------------------------------------------------------
// Subsystem 5: Sensor Fusion Extended Kalman Filter
// -----------------------------------------------------------------------------
void PrototypeBackend::runKalmanFusionStep(double gpsLat, double gpsLon, float accelX, float accelY) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    const uint64_t nowMs = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());

    // 1. Prediction step with IMU accelerometer
    m_kalmanLocator->predict(accelX, accelY, nowMs);

    // 2. Measurement update with GPS
    m_kalmanLocator->updateGps(gpsLat, gpsLon, 5.0, nowMs);

    // 3. Read fused estimate
    auto estimate = m_kalmanLocator->getEstimate();

    m_statusText = QStringLiteral("EKF Fused: [%1, %2] ±%3m")
        .arg(estimate.latitude, 0, 'f', 5)
        .arg(estimate.longitude, 0, 'f', 5)
        .arg(estimate.horizontalAccuracyMeters, 0, 'f', 1);

    emit statusChanged(m_statusText);
    emit locationFused(estimate.latitude, estimate.longitude, estimate.speedMps, estimate.horizontalAccuracyMeters);

    appendLog(QStringLiteral("[EKF Fusion] Lat: %1, Lon: %2, Speed: %3 m/s, Acc: ±%4 m")
        .arg(estimate.latitude, 0, 'f', 5)
        .arg(estimate.longitude, 0, 'f', 5)
        .arg(estimate.speedMps, 0, 'f', 2)
        .arg(estimate.horizontalAccuracyMeters, 0, 'f', 1));
}

void PrototypeBackend::notifyAppLifecycleState(const QString &state) {
    const QString msg = QStringLiteral("[Lifecycle] Application state transitioned to: %1").arg(state);
    appendLog(msg);
    qInfo().noquote() << "[GeoRescuX Qt Prototype]" << msg;
}

void PrototypeBackend::appendLog(const QString &msg) {
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    const QString formatted = QStringLiteral("[%1] %2").arg(timestamp, msg);
    emit logMessageEmitted(formatted);
}
