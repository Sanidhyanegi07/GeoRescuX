#include "AppState.h"
#include "platform/storage/SqliteEmergencyRepository.h"
#include <QDebug>

namespace georescux::app {

AppState::AppState(
    std::shared_ptr<georescux::storage::LocalDatabase> database,
    QObject *parent
)
    : AppState(std::move(database), nullptr, parent)
{
}

AppState::AppState(
    std::shared_ptr<georescux::storage::LocalDatabase> database,
    std::shared_ptr<georescux::domain::IEmergencyRepository> emergencyRepository,
    QObject *parent
)
    : QObject(parent)
    , m_database(std::move(database))
    , m_emergencyRepository(std::move(emergencyRepository))
{
    if (!m_emergencyRepository && m_database) {
        m_emergencyRepository = std::make_shared<georescux::storage::SqliteEmergencyRepository>(m_database);
    }
    m_emergencyService = std::make_shared<georescux::domain::EmergencyService>(m_emergencyRepository, this);

    setupConnections();

    if (m_database) {
        if (m_database->isInitialized() && m_database->isOpen()) {
            m_persistenceStatus = PersistenceStatus::Ready;
        } else if (!m_database->lastError().isEmpty()) {
            m_persistenceStatus = PersistenceStatus::Failed;
        } else {
            m_persistenceStatus = PersistenceStatus::NotInitialized;
        }
    } else {
        m_persistenceStatus = PersistenceStatus::NotInitialized;
    }
}

void AppState::setupConnections() {
    connect(&m_session, &georescux::domain::SessionModel::sessionStateChanged,
            this, &AppState::handleSessionStateChanged);

    if (m_emergencyService) {
        connect(m_emergencyService.get(), &georescux::domain::EmergencyService::activeEmergencyChanged,
                this, [this]() {
            const bool active = m_emergencyService->hasActiveEmergency();
            const auto newLifecycle = active ? EmergencyLifecycleState::Active : EmergencyLifecycleState::Inactive;
            if (m_emergencyState != newLifecycle) {
                m_emergencyState = newLifecycle;
                emit emergencyStateChanged(m_emergencyState);
            }
        });
    }
}

QString AppState::connectivityStatusName() const noexcept {
    switch (m_connectivity) {
        case ConnectivityState::Unknown:         return QStringLiteral("Unknown (Deferred to Phase 7)");
        case ConnectivityState::Disconnected:    return QStringLiteral("Offline");
        case ConnectivityState::ConnectedOnline: return QStringLiteral("Online (Cloud Connected)");
        case ConnectivityState::BleMeshOnly:     return QStringLiteral("P2P BLE Mesh Active");
    }
    return QStringLiteral("Unknown");
}

QString AppState::emergencyStatusName() const noexcept {
    switch (m_emergencyState) {
        case EmergencyLifecycleState::Inactive:  return QStringLiteral("Inactive");
        case EmergencyLifecycleState::Countdown: return QStringLiteral("Countdown (2s Hold)");
        case EmergencyLifecycleState::Active:    return QStringLiteral("ACTIVE EMERGENCY");
        case EmergencyLifecycleState::Completed: return QStringLiteral("Completed / Cleared");
    }
    return QStringLiteral("Unknown");
}

QString AppState::persistenceStatusName() const noexcept {
    switch (m_persistenceStatus) {
        case PersistenceStatus::NotInitialized: return QStringLiteral("Not Initialized");
        case PersistenceStatus::Ready:          return QStringLiteral("Ready (SQLite V2)");
        case PersistenceStatus::Failed:         return QStringLiteral("Initialization Failed");
    }
    return QStringLiteral("Unknown");
}

QString AppState::activePortalName() const noexcept {
    if (!m_session.isAuthenticated()) {
        return QStringLiteral("Unauthenticated Area");
    }
    if (m_session.isAdmin()) {
        return QStringLiteral("Admin Command Portal");
    }
    return QStringLiteral("Citizen User Portal");
}

bool AppState::isDestinationAllowed(const QString &dest) const {
    // Prototype Lab is accessible in all states for engineering inspection
    if (dest == QStringLiteral("PrototypeLab")) {
        return true;
    }

    if (!m_session.isAuthenticated()) {
        return (dest == QStringLiteral("Welcome") || dest == QStringLiteral("Register"));
    }

    if (m_session.isAdmin()) {
        return (dest == QStringLiteral("AdminDashboard") ||
                dest == QStringLiteral("AdminMap") ||
                dest == QStringLiteral("AdminSos") ||
                dest == QStringLiteral("AdminIncidents") ||
                dest == QStringLiteral("AdminHazards") ||
                dest == QStringLiteral("AdminEvents") ||
                dest == QStringLiteral("AdminRoutes") ||
                dest == QStringLiteral("AdminUsers") ||
                dest == QStringLiteral("AdminVerification") ||
                dest == QStringLiteral("AdminAlerts") ||
                dest == QStringLiteral("UserProfile") ||
                dest == QStringLiteral("UserHelp"));
    }

    // Citizen User
    return (dest == QStringLiteral("UserHome") ||
            dest == QStringLiteral("UserMap") ||
            dest == QStringLiteral("UserSafeRoute") ||
            dest == QStringLiteral("UserAlerts") ||
            dest == QStringLiteral("UserContacts") ||
            dest == QStringLiteral("UserProfile") ||
            dest == QStringLiteral("UserHelp"));
}

bool AppState::navigateTo(const QString &destination) {
    if (!isDestinationAllowed(destination)) {
        const QString reason = QStringLiteral("Access to '") + destination +
                               QStringLiteral("' denied for role '") + m_session.roleName() + QStringLiteral("'.");
        qWarning().noquote() << "[AppState] Navigation denied:" << reason;
        emit navigationDenied(destination, reason);
        return false;
    }

    if (m_currentNavigation != destination) {
        m_currentNavigation = destination;
        emit navigationChanged(m_currentNavigation);
        qInfo().noquote() << "[AppState] Navigated to:" << m_currentNavigation;
    }
    return true;
}

void AppState::handleSessionStateChanged(georescux::domain::SessionState newState) {
    switch (newState) {
        case georescux::domain::SessionState::Unauthenticated:
        case georescux::domain::SessionState::SessionError:
            if (m_emergencyService) {
                m_emergencyService->clearActive();
            }
            if (m_database && m_database->isOpen()) {
                m_database->clearSession();
            }
            m_currentNavigation = QStringLiteral("Welcome");
            emit navigationChanged(m_currentNavigation);
            break;

        case georescux::domain::SessionState::AuthenticatedUser:
            if (m_emergencyService) {
                m_emergencyService->loadActiveForUser(m_session.currentUserId());
            }
            m_currentNavigation = QStringLiteral("UserHome");
            emit navigationChanged(m_currentNavigation);
            break;

        case georescux::domain::SessionState::AuthenticatedAdmin:
            if (m_emergencyService) {
                m_emergencyService->clearActive();
            }
            m_currentNavigation = QStringLiteral("AdminDashboard");
            emit navigationChanged(m_currentNavigation);
            break;

        case georescux::domain::SessionState::SessionRestoring:
            break;
    }
}

bool AppState::activateSos(const QString &note) {
    if (!m_session.isAuthenticated() || m_session.currentUserId().isEmpty()) {
        qWarning() << "[AppState] Cannot activate SOS: user is not authenticated.";
        return false;
    }
    if (!m_emergencyService) {
        return false;
    }
    return m_emergencyService->activateEmergency(m_session.currentUserId(), note);
}

bool AppState::resolveSos() {
    if (!m_session.isAuthenticated() || m_session.currentUserId().isEmpty()) {
        qWarning() << "[AppState] Cannot resolve SOS: user is not authenticated.";
        return false;
    }
    if (!m_emergencyService || !m_emergencyService->hasActiveEmergency()) {
        qWarning() << "[AppState] No active SOS to resolve.";
        return false;
    }
    return m_emergencyService->completeEmergency(m_session.currentUserId(), m_emergencyService->activeEmergencyId());
}

bool AppState::loginDevelopmentUser(const QString &name, const QString &email) {
    if (m_session.loginDevelopmentUser(name, email)) {
        if (m_database && m_database->isOpen()) {
            m_database->saveSession(m_session.currentUser());
        }
        if (m_emergencyService) {
            m_emergencyService->loadActiveForUser(m_session.currentUserId());
        }
        return true;
    }
    return false;
}

bool AppState::loginDevelopmentAdmin(const QString &name, const QString &email) {
    if (m_session.loginDevelopmentAdmin(name, email)) {
        if (m_database && m_database->isOpen()) {
            m_database->saveSession(m_session.currentUser());
        }
        if (m_emergencyService) {
            m_emergencyService->clearActive();
        }
        return true;
    }
    return false;
}

void AppState::logout() {
    m_session.logout();
    if (m_emergencyService) {
        m_emergencyService->clearActive();
    }
    if (m_database && m_database->isOpen()) {
        m_database->clearSession();
    }
}

bool AppState::initializeSessionFromPersistence() {
    if (!m_database || !m_database->isOpen()) {
        return false;
    }

    auto cached = m_database->loadSession();
    if (cached.has_value() && cached->isValid()) {
        // Security Boundary: Cached admin role is NEVER proof of Admin authorization.
        if (cached->role == georescux::domain::UserRole::AuthorizedAdmin) {
            qWarning().noquote() << "[AppState] Security Policy Violation: Purging cached Admin session from local database for UID:"
                                 << cached->uid << ". Explicit administrative re-authentication required.";
            m_database->clearSession();
            return false;
        }

        qInfo().noquote() << "[AppState] Restoring cached session for user:" << cached->displayName;
        if (m_session.restoreSession(cached.value())) {
            if (m_emergencyService) {
                m_emergencyService->loadActiveForUser(m_session.currentUserId());
            }
            return true;
        }
    }
    return false;
}

} // namespace georescux::app
