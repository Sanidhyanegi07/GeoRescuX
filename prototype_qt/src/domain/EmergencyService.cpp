#include "EmergencyService.h"
#include <QDebug>

namespace georescux::domain {

EmergencyService::EmergencyService(
    std::shared_ptr<IEmergencyRepository> repository,
    QObject *parent
)
    : QObject(parent)
    , m_repository(std::move(repository))
{
}

QString EmergencyService::activeEmergencyId() const noexcept {
    return m_activeRecord.has_value() ? m_activeRecord->emergencyId : QString();
}

QString EmergencyService::activeStatusName() const noexcept {
    return m_activeRecord.has_value() ? emergencyStatusToString(m_activeRecord->status) : QStringLiteral("Inactive");
}

qint64 EmergencyService::activeStartedAtUtcMs() const noexcept {
    return m_activeRecord.has_value() ? m_activeRecord->startedAtUtcMs : 0;
}

QString EmergencyService::activeStartedTimeString() const noexcept {
    if (!m_activeRecord.has_value() || m_activeRecord->startedAtUtcMs <= 0) {
        return QString();
    }
    return QDateTime::fromMSecsSinceEpoch(m_activeRecord->startedAtUtcMs).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
}

QString EmergencyService::activeNote() const noexcept {
    return m_activeRecord.has_value() ? m_activeRecord->note : QString();
}

QString EmergencyService::activeLocationString() const noexcept {
    if (!m_activeRecord.has_value() || !m_activeRecord->location.has_value()) {
        return QStringLiteral("Coordinates Pending (Hardware GPS deferred to Phase 4)");
    }
    const auto &loc = m_activeRecord->location.value();
    return QStringLiteral("Lat: %1, Lon: %2 (±%3m)")
        .arg(loc.latitude, 0, 'f', 5)
        .arg(loc.longitude, 0, 'f', 5)
        .arg(loc.accuracyMeters, 0, 'f', 1);
}

void EmergencyService::setActiveRecord(const std::optional<EmergencyRecord> &record) {
    const bool wasActive = m_activeRecord.has_value();
    const bool nowActive = record.has_value();
    const QString oldId = wasActive ? m_activeRecord->emergencyId : QString();
    const QString newId = nowActive ? record->emergencyId : QString();

    m_activeRecord = record;

    if (wasActive != nowActive || oldId != newId) {
        emit activeEmergencyChanged();
    }
}

bool EmergencyService::activateEmergency(const QString &userId, const QString &note) {
    if (!m_repository) {
        m_lastError = QStringLiteral("Emergency repository is not available.");
        emit errorOccurred(m_lastError);
        return false;
    }

    auto res = m_repository->activateEmergency(userId, note);
    if (!res.isSuccess) {
        m_lastError = res.errorMessage;
        emit errorOccurred(m_lastError);
        qWarning().noquote() << "[EmergencyService] Activation failed:" << res.errorMessage;

        // If already active, restore that existing active record
        if (res.errorCode == EmergencyErrorCode::AlreadyActive && res.data.has_value()) {
            setActiveRecord(res.data.value());
        }
        return false;
    }

    m_lastError.clear();
    setActiveRecord(res.data.value());
    emit emergencyActivated(res.data->emergencyId);
    qInfo().noquote() << "[EmergencyService] Active emergency established:" << res.data->emergencyId;
    return true;
}

bool EmergencyService::completeEmergency(const QString &userId, const QString &emergencyId) {
    if (!m_repository) {
        m_lastError = QStringLiteral("Emergency repository is not available.");
        emit errorOccurred(m_lastError);
        return false;
    }

    auto res = m_repository->completeEmergency(userId, emergencyId);
    if (!res.isSuccess) {
        m_lastError = res.errorMessage;
        emit errorOccurred(m_lastError);
        qWarning().noquote() << "[EmergencyService] Completion failed:" << res.errorMessage;
        return false;
    }

    m_lastError.clear();
    setActiveRecord(std::nullopt);
    emit emergencyCompleted(emergencyId);
    qInfo().noquote() << "[EmergencyService] Completed emergency resolved:" << emergencyId;
    return true;
}

bool EmergencyService::updateActiveNote(const QString &note) {
    if (!m_activeRecord.has_value()) {
        m_lastError = QStringLiteral("No active emergency to update.");
        emit errorOccurred(m_lastError);
        return false;
    }
    return updateActiveNote(m_activeRecord->userId, m_activeRecord->emergencyId, note);
}

bool EmergencyService::updateActiveNote(const QString &userId, const QString &emergencyId, const QString &note) {
    if (!m_repository) {
        m_lastError = QStringLiteral("Emergency repository is not available.");
        emit errorOccurred(m_lastError);
        return false;
    }

    auto res = m_repository->updateNote(userId, emergencyId, note);
    if (!res.isSuccess) {
        m_lastError = res.errorMessage;
        emit errorOccurred(m_lastError);
        return false;
    }

    m_lastError.clear();
    setActiveRecord(res.data.value());
    return true;
}

bool EmergencyService::updateLocation(
    const QString &userId,
    const QString &emergencyId,
    const EmergencyLocation &location,
    EmergencyLocationStatus status
) {
    if (!m_repository) {
        m_lastError = QStringLiteral("Emergency repository is not available.");
        emit errorOccurred(m_lastError);
        return false;
    }

    auto res = m_repository->updateLocation(userId, emergencyId, location, status);
    if (!res.isSuccess) {
        m_lastError = res.errorMessage;
        emit errorOccurred(m_lastError);
        return false;
    }

    m_lastError.clear();
    setActiveRecord(res.data.value());
    return true;
}

void EmergencyService::loadActiveForUser(const QString &userId) {
    if (!m_repository || userId.trimmed().isEmpty()) {
        setActiveRecord(std::nullopt);
        return;
    }

    auto res = m_repository->getActiveEmergency(userId);
    if (res.isSuccess && res.data.has_value() && res.data.value().has_value()) {
        const auto &rec = res.data.value().value();
        setActiveRecord(rec);
        qInfo().noquote() << "[EmergencyService] Restored active emergency for user" << userId << ":" << rec.emergencyId;
    } else {
        setActiveRecord(std::nullopt);
    }
}

void EmergencyService::clearActive() {
    setActiveRecord(std::nullopt);
    m_lastError.clear();
}

} // namespace georescux::domain
