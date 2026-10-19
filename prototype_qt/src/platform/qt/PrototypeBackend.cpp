#include "PrototypeBackend.h"
#include "GeodesicMath.h"
#include "RouteGraph.h"
#include "AStarPathfinder.h"
#include "MeshPacketCodec.h"
#include "PacketAuthenticator.h"
#include "EpidemicRouter.h"
#include "TriageEngine.h"
#include "CasualtySurvivalProbability.h"
#include "KDTree.h"
#include "KalmanFusedLocator.h"
#include "AcousticResonanceDetector.h"

#include <QDebug>
#include <QDateTime>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>

using namespace georescux::spatial;
using namespace georescux::routing;
using namespace georescux::mesh;
using namespace georescux::crypto;
using namespace georescux::triage;
using namespace georescux::analytics;
using namespace georescux::telemetry;
using namespace georescux::audio;

PrototypeBackend::PrototypeBackend(QObject *parent)
    : QObject(parent),
      m_statusText(QStringLiteral("Ready — Native C++20 Core Loaded")),
      m_routeSummary(QStringLiteral("No route calculated. Run A* routing benchmark below.")),
      m_meshSummary(QStringLiteral("No mesh packet processed yet.")),
      m_triageSummary(QStringLiteral("No casualty assessed yet.")),
      m_spatialSummary(QStringLiteral("No spatial query executed yet.")),
      m_acousticSummary(QStringLiteral("Acoustic listener standby (16 kHz PCM FFT ready).")),
      m_kalmanLocator(std::make_unique<KalmanFusedLocator>(30.3165, 78.0322, 650.0)),
      m_epidemicRouter(std::make_unique<EpidemicRouter>("GEORESCUX_LOCAL_NODE", 100, 3600)),
      m_acousticDetector(std::make_unique<AcousticResonanceDetector>(16000, 1024))
{
    appendLog(QStringLiteral("Native C++20 GeoRescuX Engine fully initialized (Routing, Mesh, Triage, Telemetry, Acoustic)."));
}

PrototypeBackend::~PrototypeBackend() = default;

QString PrototypeBackend::statusText() const noexcept { return m_statusText; }
int PrototypeBackend::triggerCount() const noexcept { return m_triggerCount; }
double PrototypeBackend::lastComputedDistance() const noexcept { return m_lastComputedDistance; }
QString PrototypeBackend::routeSummary() const noexcept { return m_routeSummary; }
QString PrototypeBackend::meshSummary() const noexcept { return m_meshSummary; }
QString PrototypeBackend::triageSummary() const noexcept { return m_triageSummary; }
QString PrototypeBackend::spatialSummary() const noexcept { return m_spatialSummary; }
QString PrototypeBackend::acousticSummary() const noexcept { return m_acousticSummary; }

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
// Subsystem 1 (Advanced): A* Evacuation Route Graph Search & Navigation Cues
// -----------------------------------------------------------------------------
void PrototypeBackend::runAStarRoutingBenchmark(bool avoidHazards) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

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
        hazards.push_back(RoadHazard{"HAZ_01", "DED_CLOCK_TWR", "JOLLY_GRANT_HOSP", 50000.0});
    }

    RouteGraph graph(std::move(nodes), std::move(edges), std::move(hazards), 1);
    auto routeOpt = AStarPathfinder::findRoute(graph, "DED_ISBT", "RISHIKESH_CAMP");

    if (routeOpt.has_value() && !routeOpt->empty()) {
        const auto& route = *routeOpt;
        QString pathStr;
        QVariantList cues;

        for (size_t i = 0; i < route.nodeIds.size(); ++i) {
            if (i > 0) pathStr += QStringLiteral(" ➔ ");
            pathStr += QString::fromStdString(route.nodeIds[i]);

            if (i + 1 < route.nodeIds.size()) {
                QVariantMap cue;
                cue["from"] = QString::fromStdString(route.nodeIds[i]);
                cue["to"] = QString::fromStdString(route.nodeIds[i + 1]);
                cue["stepNumber"] = static_cast<int>(i + 1);
                cue["instruction"] = QStringLiteral("Proceed from %1 toward %2")
                    .arg(QString::fromStdString(route.nodeIds[i]))
                    .arg(QString::fromStdString(route.nodeIds[i + 1]));
                cues.append(cue);
            }
        }

        const int minutes = static_cast<int>(route.estimatedDurationSeconds / 60);
        m_routeSummary = QStringLiteral("Path: %1 | Distance: %2 km | Cost: %3 km | Est: %4 min")
            .arg(pathStr)
            .arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2)
            .arg(route.totalCostMeters / 1000.0, 0, 'f', 2)
            .arg(minutes);

        QString warningsStr = route.hazardWarnings.empty() ? QStringLiteral("None (Clear Road)")
            : QStringLiteral("%1 hazard(s) bypassed (Thano Bypass Active)").arg(route.hazardWarnings.size());

        emit routeSummaryChanged(m_routeSummary);
        emit routeCalculated(pathStr, route.totalDistanceMeters, route.totalCostMeters, static_cast<int>(route.estimatedDurationSeconds), warningsStr, cues);

        appendLog(QStringLiteral("[A* Routing] Evacuation Corridor: %1 (%2 km, %3 min)")
            .arg(pathStr).arg(route.totalDistanceMeters / 1000.0, 0, 'f', 2).arg(minutes));
    } else {
        m_routeSummary = QStringLiteral("No viable route found between origin and destination.");
        emit routeSummaryChanged(m_routeSummary);
        appendLog(QStringLiteral("[A* Routing] No viable route found."));
    }
}

// -----------------------------------------------------------------------------
// Subsystem 2: Mesh Packet Codec & HMAC Authenticator
// -----------------------------------------------------------------------------
void PrototypeBackend::runMeshPacketBenchmark(int urgency, int battery, int peopleCount) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    const std::string senderId = "NODE_RESCUE_07";
    const double lat = 30.3165;
    const double lon = 78.0322;
    const double alt = 645.0;
    const uint32_t nowSec = static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());

    auto packetBytes = MeshPacketCodec::encodeSosPacket(
        static_cast<uint8_t>(MeshPriority::CRITICAL),
        15,
        static_cast<uint16_t>(m_triggerCount),
        senderId,
        lat, lon, alt,
        static_cast<uint8_t>(battery),
        static_cast<uint8_t>(urgency),
        static_cast<uint16_t>(peopleCount),
        nowSec
    );

    std::stringstream hexStream;
    for (uint8_t b : packetBytes) {
        hexStream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    const std::string hexStr = hexStream.str();

    auto decodedOpt = MeshPacketCodec::decodePacket(packetBytes.data(), packetBytes.size());
    const bool crcValid = decodedOpt.has_value();

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

    m_meshSummary = QStringLiteral("Size: %1 B | CRC-16: %2 | HMAC: %3...")
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
// Subsystem 2 (Advanced): Epidemic DTN Mesh Relay Simulator
// -----------------------------------------------------------------------------
void PrototypeBackend::runEpidemicMeshRelayBenchmark() {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    const uint32_t nowSec = static_cast<uint32_t>(QDateTime::currentSecsSinceEpoch());

    // 1. Enqueue local SOS
    m_epidemicRouter->enqueueLocalSos(30.3165, 78.0322, 640.0, 90, 3, 2, nowSec);

    // 2. Ingest peer packets from surrounding rescue nodes
    auto peerPacket1 = MeshPacketCodec::encodeSosPacket(
        static_cast<uint8_t>(MeshPriority::HIGH), 10, 101, "PEER_SCOUT_ALPHA",
        30.3255, 78.0436, 642.0, 85, 2, 1, nowSec
    );
    auto peerPacket2 = MeshPacketCodec::encodeSosPacket(
        static_cast<uint8_t>(MeshPriority::CRITICAL), 12, 102, "PEER_DISPATCH_BRAVO",
        30.1878, 78.1884, 550.0, 72, 3, 5, nowSec
    );

    const bool accepted1 = m_epidemicRouter->ingestPacket(peerPacket1.data(), peerPacket1.size());
    const bool accepted2 = m_epidemicRouter->ingestPacket(peerPacket2.data(), peerPacket2.size());

    // Ingest duplicate of peerPacket1 to verify BloomFilter deduplicator suppression
    const bool duplicateAccepted = m_epidemicRouter->ingestPacket(peerPacket1.data(), peerPacket1.size());

    // 3. Prepare transmission batch (decrement TTL, forward)
    auto txBatch = m_epidemicRouter->getPacketsForTransmission(4);
    const size_t queueRemaining = m_epidemicRouter->getQueueSize();

    const QString relayReport = QStringLiteral("Queue: %1 pkts | TX Forwarded: %2 | Ingested: %3 | Bloom Suppressed: %4")
        .arg(queueRemaining)
        .arg(txBatch.size())
        .arg((accepted1 ? 1 : 0) + (accepted2 ? 1 : 0))
        .arg(duplicateAccepted ? 0 : 1);

    m_meshSummary = relayReport;
    emit meshSummaryChanged(m_meshSummary);
    emit meshRelayUpdated(static_cast<int>(queueRemaining), static_cast<int>(txBatch.size()), duplicateAccepted ? 0 : 1, relayReport);

    appendLog(QStringLiteral("[Epidemic Relay] %1").arg(relayReport));
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

    auto result = TriageEngine::evaluateStart(assessment);
    const uint8_t urgencyScore = TriageEngine::computeContinuousUrgencyScore(
        result.category, hemorrhage, trapped, static_cast<uint8_t>(respRate), radialPulse
    );

    CasualtyProfile profile;
    profile.ageYears = 34;
    profile.trappedDurationHours = trapped ? 3.5 : 0.5;
    profile.ambientTemperatureCelsius = 4.0;
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
// Subsystem 4: Spatial KD-Tree Shelter Lookup & Map Coordinate Projections
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

QVariantList PrototypeBackend::getMapWaypoints() const {
    QVariantList list;
    auto addWp = [&](const QString &id, double lat, double lon) {
        QVariantMap m;
        m["id"] = id;
        m["lat"] = lat;
        m["lon"] = lon;
        list.append(m);
    };
    addWp("DED_ISBT", 30.2858, 78.0098);
    addWp("DED_CLOCK_TWR", 30.3255, 78.0436);
    addWp("MALSI_SHELTER", 30.3789, 78.0772);
    addWp("JOLLY_GRANT_HOSP", 30.1878, 78.1884);
    addWp("RISHIKESH_CAMP", 30.1033, 78.2947);
    addWp("HARIDWAR_HQ", 29.9457, 78.1642);
    return list;
}

QVariantList PrototypeBackend::getMapShelters() const {
    QVariantList list;
    auto addSh = [&](const QString &name, double lat, double lon, int capacity) {
        QVariantMap m;
        m["name"] = name;
        m["lat"] = lat;
        m["lon"] = lon;
        m["capacity"] = capacity;
        list.append(m);
    };
    addSh("Malsi Staging Depot", 30.3789, 78.0772, 850);
    addSh("Jolly Grant Hospital", 30.1878, 78.1884, 1200);
    addSh("Rishikesh Evac Camp", 30.1033, 78.2947, 3000);
    return list;
}

QVariantMap PrototypeBackend::getActiveHazardZone() const {
    QVariantMap m;
    m["name"] = "Thano Flash Flood & Landslide Zone";
    m["centerLat"] = 30.25;
    m["centerLon"] = 78.12;
    m["radiusMeters"] = 6500.0;
    return m;
}

// -----------------------------------------------------------------------------
// Subsystem 5: Sensor Fusion Extended Kalman Filter
// -----------------------------------------------------------------------------
void PrototypeBackend::runKalmanFusionStep(double gpsLat, double gpsLon, float accelX, float accelY) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    const uint64_t nowMs = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());

    m_kalmanLocator->predict(accelX, accelY, nowMs);
    m_kalmanLocator->updateGps(gpsLat, gpsLon, 5.0, nowMs);

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

// -----------------------------------------------------------------------------
// Subsystem 6: Acoustic Resonance & Rubble Tapping Detector
// -----------------------------------------------------------------------------
void PrototypeBackend::runAcousticDetectionBenchmark(bool simulateWhistle) {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    constexpr size_t sampleRate = 16000;
    constexpr size_t sampleCount = 4096;
    std::vector<int16_t> pcmAudio(sampleCount, 0);

    // Synthesize realistic microphone audio buffer
    std::mt19937 rng(1337);
    std::normal_distribution<double> noiseDist(0.0, 150.0); // Ambient noise

    for (size_t i = 0; i < sampleCount; ++i) {
        double signal = noiseDist(rng);

        if (simulateWhistle) {
            // High-pitched rescue whistle at 2950 Hz (classic pea-less disaster whistle resonance)
            const double t = static_cast<double>(i) / sampleRate;
            signal += 6500.0 * std::sin(2.0 * M_PI * 2950.0 * t);
        }

        signal = std::clamp(signal, -32767.0, 32767.0);
        pcmAudio[i] = static_cast<int16_t>(signal);
    }

    const uint64_t startMs = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch());
    m_acousticDetector->processPcmAudio(pcmAudio.data(), pcmAudio.size(), startMs);

    auto peaks = m_acousticDetector->getResonantPeaks(-35.0, 3);
    auto tappingResult = m_acousticDetector->analyzeTappingPatterns();

    QString peaksSummary;
    for (size_t i = 0; i < peaks.size(); ++i) {
        if (i > 0) peaksSummary += QStringLiteral(", ");
        peaksSummary += QStringLiteral("%1 Hz (%2 dB)")
            .arg(peaks[i].frequencyHz, 0, 'f', 0)
            .arg(peaks[i].magnitudeDb, 0, 'f', 1);
    }
    if (peaks.empty()) {
        peaksSummary = QStringLiteral("No resonant peaks detected above threshold.");
    }

    m_acousticSummary = QStringLiteral("Peaks: %1 | Rhythmic: %2 | SOS Morse: %3")
        .arg(peaksSummary)
        .arg(tappingResult.isRhythmicTappingDetected ? QStringLiteral("YES") : QStringLiteral("NO"))
        .arg(tappingResult.isSosMorseCode ? QStringLiteral("DETECTED") : QStringLiteral("NONE"));

    emit acousticSummaryChanged(m_acousticSummary);
    emit acousticPatternDetected(
        tappingResult.isRhythmicTappingDetected,
        tappingResult.isSosMorseCode,
        tappingResult.estimatedPeriodMs,
        tappingResult.patternRegularityScore,
        peaksSummary
    );

    appendLog(QStringLiteral("[Acoustic Detector] Resonance: %1 | Morse SOS: %2")
        .arg(peaksSummary)
        .arg(tappingResult.isSosMorseCode ? QStringLiteral("ACTIVE") : QStringLiteral("INACTIVE")));
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
