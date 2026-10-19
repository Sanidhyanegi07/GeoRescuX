#ifndef GEORESCUX_PROTOTYPE_BACKEND_H
#define GEORESCUX_PROTOTYPE_BACKEND_H

#include <QObject>
#include <QString>
#include <QDateTime>

/**
 * @brief C++20 backend object exposing Q_INVOKABLE methods and signals to QML.
 * 
 * Verifies that:
 * 1. Qt Quick / QML can call native C++ methods directly.
 * 2. C++ can execute portable business/spatial logic (GeodesicMath).
 * 3. Signals emitted from C++ reliably update the QML interface.
 * 4. Runtime diagnostics and lifecycle state changes are cleanly logged.
 */
class PrototypeBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(int triggerCount READ triggerCount NOTIFY triggerCountChanged)
    Q_PROPERTY(double lastComputedDistance READ lastComputedDistance NOTIFY calculationFinished)

public:
    explicit PrototypeBackend(QObject *parent = nullptr);

    QString statusText() const noexcept;
    int triggerCount() const noexcept;
    double lastComputedDistance() const noexcept;

    /**
     * @brief Invoked from QML upon user interaction (button tap).
     * Calculates geodesic Haversine and Vincenty distance using the existing
     * GeoRescuX native GeodesicMath engine.
     */
    Q_INVOKABLE void runGeodesicBenchmark();

    /**
     * @brief Simulates lifecycle pause/resume logging.
     */
    Q_INVOKABLE void notifyAppLifecycleState(const QString &state);

signals:
    void statusChanged(const QString &newStatus);
    void triggerCountChanged(int newCount);
    void calculationFinished(double distanceMeters, const QString &details);
    void logMessageEmitted(const QString &formattedMessage);

private:
    QString m_statusText;
    int m_triggerCount{0};
    double m_lastComputedDistance{0.0};

    void appendLog(const QString &msg);
};

#endif // GEORESCUX_PROTOTYPE_BACKEND_H
