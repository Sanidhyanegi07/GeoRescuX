#include "PrototypeBackend.h"
#include "GeodesicMath.h"

#include <QDebug>
#include <QDateTime>

PrototypeBackend::PrototypeBackend(QObject *parent)
    : QObject(parent),
      m_statusText(QStringLiteral("Ready — Backend Initialized"))
{
    appendLog(QStringLiteral("PrototypeBackend initialized. C++20 engine ready."));
}

QString PrototypeBackend::statusText() const noexcept {
    return m_statusText;
}

int PrototypeBackend::triggerCount() const noexcept {
    return m_triggerCount;
}

double PrototypeBackend::lastComputedDistance() const noexcept {
    return m_lastComputedDistance;
}

void PrototypeBackend::runGeodesicBenchmark() {
    m_triggerCount++;
    emit triggerCountChanged(m_triggerCount);

    // Test coordinates: Dehradun to Rishikesh (Uttarakhand evacuation route test corridor)
    constexpr double lat1 = 30.3165; // Dehradun center
    constexpr double lon1 = 78.0322;
    constexpr double lat2 = 30.0869; // Rishikesh center
    constexpr double lon2 = 78.2676;

    // Call existing native C++20 GeodesicMath routine
    const double haversineMeters = georescux::spatial::GeodesicMath::haversineDistanceMeters(
        lat1, lon1, lat2, lon2
    );
    const double vincentyMeters = georescux::spatial::GeodesicMath::vincentyDistanceMeters(
        lat1, lon1, lat2, lon2
    );
    const double bearing = georescux::spatial::GeodesicMath::initialBearingDegrees(
        lat1, lon1, lat2, lon2
    );

    m_lastComputedDistance = vincentyMeters;
    const double diffMeters = std::abs(haversineMeters - vincentyMeters);

    const QString details = QStringLiteral("Vincenty: %1 m | Haversine: %2 m | Delta: %3 m | Initial Bearing: %4°")
        .arg(vincentyMeters, 0, 'f', 2)
        .arg(haversineMeters, 0, 'f', 2)
        .arg(diffMeters, 0, 'f', 3)
        .arg(bearing, 0, 'f', 1);

    m_statusText = QStringLiteral("Calculation #%1 Complete: %2 m").arg(m_triggerCount).arg(vincentyMeters, 0, 'f', 1);
    emit statusChanged(m_statusText);
    emit calculationFinished(vincentyMeters, details);

    appendLog(QStringLiteral("[Benchmark #%1] %2").arg(m_triggerCount).arg(details));
    qInfo().noquote() << "[GeoRescuX Qt Prototype]" << details;
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
