#ifndef GEORESCUX_EMERGENCY_SERVICE_H
#define GEORESCUX_EMERGENCY_SERVICE_H

#include <QObject>
#include <QString>
#include <memory>
#include <optional>
#include "EmergencyModel.h"
#include "IEmergencyRepository.h"

namespace georescux::domain {

/**
 * @brief Authoritative Emergency Orchestration Service and Controller.
 *
 * Exposes thread-safe, reactive emergency lifecycle states and actions
 * to C++ subsystems and Qt Quick QML views.
 */
class EmergencyService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasActiveEmergency READ hasActiveEmergency NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeEmergencyId READ activeEmergencyId NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeStatusName READ activeStatusName NOTIFY activeEmergencyChanged)
    Q_PROPERTY(qint64 activeStartedAtUtcMs READ activeStartedAtUtcMs NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeStartedTimeString READ activeStartedTimeString NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeNote READ activeNote NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeLocationString READ activeLocationString NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    explicit EmergencyService(
        std::shared_ptr<IEmergencyRepository> repository,
        QObject *parent = nullptr
    );
    ~EmergencyService() override = default;

    [[nodiscard]] bool hasActiveEmergency() const noexcept { return m_activeRecord.has_value(); }
    [[nodiscard]] QString activeEmergencyId() const noexcept;
    [[nodiscard]] QString activeStatusName() const noexcept;
    [[nodiscard]] qint64 activeStartedAtUtcMs() const noexcept;
    [[nodiscard]] QString activeStartedTimeString() const noexcept;
    [[nodiscard]] QString activeNote() const noexcept;
    [[nodiscard]] QString activeLocationString() const noexcept;
    [[nodiscard]] QString lastError() const noexcept { return m_lastError; }
    [[nodiscard]] const std::optional<EmergencyRecord> &activeRecord() const noexcept { return m_activeRecord; }
    [[nodiscard]] IEmergencyRepository* repository() const noexcept { return m_repository.get(); }

    /**
     * @brief Activates a new SOS emergency for the authenticated user.
     * Persists the record before notifying UI.
     */
    Q_INVOKABLE bool activateEmergency(const QString &userId, const QString &note = QString());

    /**
     * @brief Completes the active emergency and records completion timestamp in history.
     */
    Q_INVOKABLE bool completeEmergency(const QString &userId, const QString &emergencyId);

    /**
     * @brief Updates the note on the active emergency.
     * Enforces authenticated user ownership.
     */
    Q_INVOKABLE bool updateActiveNote(const QString &userId, const QString &emergencyId, const QString &note);
    Q_INVOKABLE bool updateActiveNote(const QString &note);

    /**
     * @brief Updates location on the active emergency.
     * Enforces authenticated user ownership.
     */
    bool updateLocation(
        const QString &userId,
        const QString &emergencyId,
        const EmergencyLocation &location,
        EmergencyLocationStatus status
    );

    /**
     * @brief Restores active emergency state from persistent SQLite for the authenticated user.
     */
    Q_INVOKABLE void loadActiveForUser(const QString &userId);

    /**
     * @brief Clears in-memory active emergency representation (e.g. on user logout).
     * Does NOT remove historical records from SQLite.
     */
    Q_INVOKABLE void clearActive();

signals:
    void activeEmergencyChanged();
    void emergencyActivated(const QString &emergencyId);
    void emergencyCompleted(const QString &emergencyId);
    void errorOccurred(const QString &errorMessage);

private:
    void setActiveRecord(const std::optional<EmergencyRecord> &record);

    std::shared_ptr<IEmergencyRepository> m_repository;
    std::optional<EmergencyRecord> m_activeRecord{std::nullopt};
    QString m_lastError;
};

} // namespace georescux::domain

#endif // GEORESCUX_EMERGENCY_SERVICE_H
