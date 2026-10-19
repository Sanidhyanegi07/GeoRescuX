#ifndef GEORESCUX_PROTOTYPE_BACKEND_H
#define GEORESCUX_PROTOTYPE_BACKEND_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <memory>

namespace georescux::telemetry {
class KalmanFusedLocator;
}

/**
 * @brief Unified Native C++20 backend object exposing full GeoRescuX subsystems to QML.
 * 
 * Direct zero-copy C++ execution of:
 * 1. Routing Engine (A*, Bidirectional A*, Hazard Avoidance)
 * 2. Mesh Networking (Bit-packed packet codec, CRC-16, HMAC-SHA256 auth, Epidemic routing)
 * 3. MCI Medical Triage (START protocol, urgency scoring, casualty survival analytics)
 * 4. Spatial Intelligence (KD-Tree safe haven search, geodesic polygons)
 * 5. Telemetry & Sensor Fusion (6-State Extended Kalman Filter)
 */
class PrototypeBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(int triggerCount READ triggerCount NOTIFY triggerCountChanged)
    Q_PROPERTY(double lastComputedDistance READ lastComputedDistance NOTIFY calculationFinished)
    Q_PROPERTY(QString routeSummary READ routeSummary NOTIFY routeSummaryChanged)
    Q_PROPERTY(QString meshSummary READ meshSummary NOTIFY meshSummaryChanged)
    Q_PROPERTY(QString triageSummary READ triageSummary NOTIFY triageSummaryChanged)
    Q_PROPERTY(QString spatialSummary READ spatialSummary NOTIFY spatialSummaryChanged)

public:
    explicit PrototypeBackend(QObject *parent = nullptr);
    ~PrototypeBackend() override;

    QString statusText() const noexcept;
    int triggerCount() const noexcept;
    double lastComputedDistance() const noexcept;
    QString routeSummary() const noexcept;
    QString meshSummary() const noexcept;
    QString triageSummary() const noexcept;
    QString spatialSummary() const noexcept;

    // Subsystem 1: Geodesic & A* Evacuation Routing
    Q_INVOKABLE void runGeodesicBenchmark();
    Q_INVOKABLE void runAStarRoutingBenchmark(bool avoidHazards = true);

    // Subsystem 2: BLE Mesh Packet Codec & Cryptographic Authenticator
    Q_INVOKABLE void runMeshPacketBenchmark(int urgency = 3, int battery = 85, int peopleCount = 4);

    // Subsystem 3: MCI Disaster Triage & Survival Analytics
    Q_INVOKABLE void runTriageAssessment(
        bool canWalk,
        bool breathing,
        int respRate,
        bool radialPulse,
        bool hemorrhage,
        bool trapped
    );

    // Subsystem 4: Spatial KD-Tree Shelter Lookup & Hazard Geometry
    Q_INVOKABLE void runSpatialShelterQuery(double lat = 30.3165, double lon = 78.0322);

    // Subsystem 5: Sensor Fusion Extended Kalman Filter
    Q_INVOKABLE void runKalmanFusionStep(double gpsLat = 30.3165, double gpsLon = 78.0322, float accelX = 0.5f, float accelY = 0.2f);

    // Lifecycle logging
    Q_INVOKABLE void notifyAppLifecycleState(const QString &state);

signals:
    void statusChanged(const QString &newStatus);
    void triggerCountChanged(int newCount);
    void calculationFinished(double distanceMeters, const QString &details);
    void routeSummaryChanged(const QString &summary);
    void meshSummaryChanged(const QString &summary);
    void triageSummaryChanged(const QString &summary);
    void spatialSummaryChanged(const QString &summary);

    void routeCalculated(const QString &nodePath, double distanceMeters, double costMeters, int durationSeconds, const QString &warnings);
    void meshPacketProcessed(const QString &packetHex, int sizeBytes, bool crcValid, const QString &hmacDigest, const QString &decodedSummary);
    void triageEvaluated(const QString &category, int score, const QString &reason, const QString &intervention, double survivalProb);
    void shelterFound(const QString &shelterName, double lat, double lon, double distanceMeters);
    void locationFused(double lat, double lon, double speedMps, double accuracyMeters);
    void logMessageEmitted(const QString &formattedMessage);

private:
    QString m_statusText;
    int m_triggerCount{0};
    double m_lastComputedDistance{0.0};
    QString m_routeSummary;
    QString m_meshSummary;
    QString m_triageSummary;
    QString m_spatialSummary;

    std::unique_ptr<georescux::telemetry::KalmanFusedLocator> m_kalmanLocator;

    void appendLog(const QString &msg);
};

#endif // GEORESCUX_PROTOTYPE_BACKEND_H
