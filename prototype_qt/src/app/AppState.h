#ifndef GEORESCUX_APP_STATE_H
#define GEORESCUX_APP_STATE_H

#include <QObject>
#include <QString>
#include <memory>
#include "domain/SessionModel.h"
#include "domain/EmergencyService.h"
#include "domain/MapPackageManager.h"
#include "domain/RouteService.h"
#include "domain/IRouteRepository.h"
#include "platform/storage/LocalDatabase.h"

namespace georescux::app {

enum class ConnectivityState {
    Unknown = 0,
    Disconnected = 1,
    ConnectedOnline = 2,
    BleMeshOnly = 3
};

enum class EmergencyLifecycleState {
    Inactive = 0,
    Countdown = 1,
    Active = 2,
    Completed = 3
};

enum class PersistenceStatus {
    NotInitialized = 0,
    Ready = 1,
    Failed = 2
};

/**
 * @brief Authoritative central application state model and navigation router.
 *
 * Enforces unified single-source-of-truth across QML views, preventing
 * divergent or simulated state across separate screens.
 */
class AppState : public QObject {
    Q_OBJECT
    Q_PROPERTY(georescux::domain::SessionModel* session READ session CONSTANT)
    Q_PROPERTY(georescux::domain::EmergencyService* emergencyService READ emergencyService CONSTANT)
    Q_PROPERTY(georescux::domain::MapPackageManager* packageManager READ packageManager CONSTANT)
    Q_PROPERTY(georescux::domain::RouteService* routeService READ routeService CONSTANT)
    Q_PROPERTY(QString currentNavigation READ currentNavigation NOTIFY navigationChanged)
    Q_PROPERTY(QString connectivityStatusName READ connectivityStatusName NOTIFY connectivityChanged)
    Q_PROPERTY(QString emergencyStatusName READ emergencyStatusName NOTIFY emergencyStateChanged)
    Q_PROPERTY(QString persistenceStatusName READ persistenceStatusName NOTIFY persistenceStatusChanged)
    Q_PROPERTY(bool isPersistenceReady READ isPersistenceReady NOTIFY persistenceStatusChanged)
    Q_PROPERTY(QString activePortalName READ activePortalName NOTIFY navigationChanged)
    Q_PROPERTY(bool isEmergencyActive READ isEmergencyActive NOTIFY emergencyStateChanged)
    Q_PROPERTY(QString activeRegionId READ activeRegionId NOTIFY activeRegionChanged)
    Q_PROPERTY(QString activeRegionDisplayName READ activeRegionDisplayName NOTIFY activeRegionChanged)
    Q_PROPERTY(bool hasActiveMapPackage READ hasActiveMapPackage NOTIFY activeRegionChanged)
    Q_PROPERTY(QString activeBoundaryPath READ activeBoundaryPath NOTIFY activeRegionChanged)
    Q_PROPERTY(QString activeGraphPath READ activeGraphPath NOTIFY activeRegionChanged)
    Q_PROPERTY(QString activePlacesPath READ activePlacesPath NOTIFY activeRegionChanged)

public:
    explicit AppState(
        std::shared_ptr<georescux::storage::LocalDatabase> database,
        QObject *parent = nullptr
    );
    AppState(
        std::shared_ptr<georescux::storage::LocalDatabase> database,
        std::shared_ptr<georescux::domain::IEmergencyRepository> emergencyRepository,
        QObject *parent = nullptr
    );
    ~AppState() override = default;

    [[nodiscard]] georescux::domain::SessionModel* session() noexcept { return &m_session; }
    [[nodiscard]] georescux::domain::EmergencyService* emergencyService() noexcept { return m_emergencyService.get(); }
    [[nodiscard]] georescux::domain::MapPackageManager* packageManager() noexcept { return m_packageManager.get(); }
    [[nodiscard]] georescux::domain::RouteService* routeService() noexcept { return m_routeService.get(); }
    [[nodiscard]] QString activeRegionId() const;
    [[nodiscard]] QString activeRegionDisplayName() const;
    [[nodiscard]] bool hasActiveMapPackage() const;
    [[nodiscard]] QString activeBoundaryPath() const;
    [[nodiscard]] QString activeGraphPath() const;
    [[nodiscard]] QString activePlacesPath() const;
    [[nodiscard]] QString currentNavigation() const noexcept { return m_currentNavigation; }
    [[nodiscard]] ConnectivityState connectivity() const noexcept { return m_connectivity; }
    [[nodiscard]] QString connectivityStatusName() const noexcept;
    [[nodiscard]] EmergencyLifecycleState emergencyState() const noexcept { return m_emergencyState; }
    [[nodiscard]] QString emergencyStatusName() const noexcept;
    [[nodiscard]] PersistenceStatus persistenceStatus() const noexcept { return m_persistenceStatus; }
    [[nodiscard]] QString persistenceStatusName() const noexcept;
    [[nodiscard]] bool isPersistenceReady() const noexcept { return m_persistenceStatus == PersistenceStatus::Ready; }
    [[nodiscard]] QString activePortalName() const noexcept;
    [[nodiscard]] bool isEmergencyActive() const noexcept { return m_emergencyState == EmergencyLifecycleState::Active; }

    /**
     * @brief Requests navigation to a target screen with strict role validation.
     * Rejects unauthorized portal transitions.
     */
    Q_INVOKABLE bool navigateTo(const QString &destination);

    /**
     * @brief Activates a real persistent SOS emergency for the currently authenticated Citizen user.
     * Explicitly rejects Admin sessions.
     */
    Q_INVOKABLE bool activateSos(const QString &note = QString());

    /**
     * @brief Completes and resolves the active SOS emergency for the authenticated Citizen user.
     * Explicitly rejects Admin sessions.
     */
    Q_INVOKABLE bool resolveSos();

    /**
     * @brief Updates note on the active SOS emergency for the authenticated Citizen user.
     * Explicitly rejects Admin sessions.
     */
    Q_INVOKABLE bool updateActiveNote(const QString &note);

    /**
     * @brief Selects and activates an installed regional map and routing package.
     */
    Q_INVOKABLE bool selectRegion(const QString &regionId);

    /**
     * @brief Calculates an offline evacuation route within the active region.
     */
    Q_INVOKABLE bool calculateRoute(double startLat, double startLon, double destLat, double destLon, bool avoidHazards = true);

    /**
     * @brief Clears active calculated route overlay.
     */
    Q_INVOKABLE void clearRoute();

    /**
     * @brief Visibly labeled test facility for role switching with session caching.
     */
    Q_INVOKABLE bool loginDevelopmentUser(const QString &name, const QString &email);
    Q_INVOKABLE bool loginDevelopmentAdmin(const QString &name, const QString &email);
    Q_INVOKABLE void logout();

    /**
     * @brief Restores persisted session from SQLite if available.
     */
    bool initializeSessionFromPersistence();

signals:
    void navigationChanged(const QString &newDestination);
    void connectivityChanged(ConnectivityState newState);
    void emergencyStateChanged(EmergencyLifecycleState newState);
    void persistenceStatusChanged(PersistenceStatus newStatus);
    void navigationDenied(const QString &requestedDestination, const QString &reason);
    void activeRegionChanged(const QString &regionId);

private:
    void setupConnections();
    void handleSessionStateChanged(georescux::domain::SessionState newState);
    bool isDestinationAllowed(const QString &destination) const;

    std::shared_ptr<georescux::storage::LocalDatabase> m_database;
    std::shared_ptr<georescux::domain::IEmergencyRepository> m_emergencyRepository;
    std::shared_ptr<georescux::domain::EmergencyService> m_emergencyService;
    std::shared_ptr<georescux::domain::MapPackageManager> m_packageManager;
    std::shared_ptr<georescux::domain::IRouteRepository> m_routeRepository;
    std::shared_ptr<georescux::domain::RouteService> m_routeService;
    georescux::domain::SessionModel m_session;
    QString m_currentNavigation{QStringLiteral("Welcome")};
    ConnectivityState m_connectivity{ConnectivityState::Unknown};
    EmergencyLifecycleState m_emergencyState{EmergencyLifecycleState::Inactive};
    PersistenceStatus m_persistenceStatus{PersistenceStatus::NotInitialized};
};

} // namespace georescux::app

#endif // GEORESCUX_APP_STATE_H
