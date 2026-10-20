#include "AppState.h"
#include "platform/storage/SqliteEmergencyRepository.h"
#include "platform/storage/SqliteMapPackageRepository.h"
#include "platform/storage/OfflineRouteRepository.h"
#include "platform/qt/QtLocationProvider.h"
#include <QFile>
#include <QDebug>

#include <QCoreApplication>
#include <QDir>

static QString resolveBundledAssetPath(const QString &relPath) {
    if (QFile::exists(relPath)) return relPath;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList prefixes = {
        QStringLiteral(""),
        QStringLiteral("../"),
        QStringLiteral("../../"),
        QStringLiteral("../../../")
    };
    for (const auto &pfx : prefixes) {
        const QString cand = pfx + relPath;
        if (QFile::exists(cand)) return cand;
        if (!appDir.isEmpty()) {
            const QString candApp = QDir(appDir).filePath(cand);
            if (QFile::exists(candApp)) return candApp;
        }
    }
    const QString androidAsset = QStringLiteral("assets:/") + relPath;
    if (QFile::exists(androidAsset)) return androidAsset;
    return QString();
}

namespace georescux::app {

AppState::AppState(
    std::shared_ptr<georescux::storage::LocalDatabase> database,
    QObject *parent
)
    : AppState(std::move(database), nullptr, nullptr, parent)
{
}

AppState::AppState(
    std::shared_ptr<georescux::storage::LocalDatabase> database,
    std::shared_ptr<georescux::domain::IEmergencyRepository> emergencyRepository,
    QObject *parent
)
    : AppState(std::move(database), std::move(emergencyRepository), nullptr, parent)
{
}

AppState::AppState(
    std::shared_ptr<georescux::storage::LocalDatabase> database,
    std::shared_ptr<georescux::domain::IEmergencyRepository> emergencyRepository,
    std::shared_ptr<georescux::domain::ILocationProvider> locationProvider,
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

    // Initialize Map Package and Routing Infrastructure
    auto pkgRepo = std::make_shared<georescux::storage::SqliteMapPackageRepository>(m_database);
    m_packageManager = std::make_shared<georescux::domain::MapPackageManager>(pkgRepo, QString(), this);
    m_routeRepository = std::make_shared<georescux::domain::OfflineRouteRepository>(m_packageManager);
    m_routeService = std::make_shared<georescux::domain::RouteService>(m_routeRepository, m_packageManager, this);

    // Initialize Location Provider & Service
    if (!locationProvider) {
        locationProvider = std::make_shared<georescux::platform::QtLocationProvider>();
    }
    m_locationService = std::make_shared<georescux::domain::LocationService>(locationProvider, m_packageManager, nullptr, this);

    connect(m_packageManager.get(), &georescux::domain::MapPackageManager::activeRegionChanged,
            this, &AppState::activeRegionChanged);

    // Initialize package: first check if already installed in SQLite persistence
    if (m_packageManager->hasActivePackage()) {
        qInfo() << "[AppState] Restored active package from persistence:" << m_packageManager->activeRegionId();
    } else {
        auto installedUk = m_packageManager->getPackage(QStringLiteral("uttarakhand"));
        if (installedUk && installedUk->status == georescux::domain::PackageStatus::Installed &&
            (installedUk->hasRenderingData || installedUk->hasRoutingData)) {
            m_packageManager->activateRegion(QStringLiteral("uttarakhand"));
            qInfo() << "[AppState] Activated existing installed package from SQLite:" << installedUk->regionId;
        }
    }

    // If no active package, attempt clean installation from bundled manifest
    if (!m_packageManager->hasActivePackage()) {
        const QStringList manifestCandidates = {
            QStringLiteral("assets:/map_packages/uttarakhand/manifest.json"),
            resolveBundledAssetPath(QStringLiteral("prototype_qt/platform/android/assets/map_packages/uttarakhand/manifest.json")),
            resolveBundledAssetPath(QStringLiteral("platform/android/assets/map_packages/uttarakhand/manifest.json")),
            resolveBundledAssetPath(QStringLiteral("output/uttarakhand/manifest.json"))
        };

        QString bundledManifest;
        for (const auto &cand : manifestCandidates) {
            if (!cand.isEmpty() && QFile::exists(cand)) {
                bundledManifest = cand;
                break;
            }
        }

        if (!bundledManifest.isEmpty()) {
            QString installError;
            if (m_packageManager->installPackage(bundledManifest, installError)) {
                m_packageManager->activateRegion(QStringLiteral("uttarakhand"));
                qInfo() << "[AppState] Clean installation successful from bundled manifest:" << bundledManifest;
            } else {
                qWarning() << "[AppState] Failed to install bundled package from" << bundledManifest << ":" << installError;
            }
        }
    }

    // In-situ development package fallback (for desktop development when assets are in repo root)
    if (!m_packageManager->hasActivePackage()) {
        const QString mapPath = resolveBundledAssetPath(QStringLiteral("app/src/main/assets/maps/uttarakhand-state.map"));
        const QString graphPath = resolveBundledAssetPath(QStringLiteral("app/src/main/assets/route_graph_uttarakhand"));
        const QString boundaryPath = resolveBundledAssetPath(QStringLiteral("output/uttarakhand/boundary.geojson"));
        const QString placesPath = resolveBundledAssetPath(QStringLiteral("app/src/main/assets/maps/uttarakhand-places.json"));

        if (!mapPath.isEmpty() || !graphPath.isEmpty()) {
            georescux::domain::MapPackage ukPkg;
            ukPkg.regionId = QStringLiteral("uttarakhand");
            ukPkg.displayName = QStringLiteral("Uttarakhand");
            ukPkg.packageVersion = 1;
            ukPkg.supportedFormat = !mapPath.isEmpty() ? QStringLiteral("Mapsforge MAP") : QStringLiteral("GeoRescuX Vector");
            ukPkg.coverage.minLatitude = 28.7;
            ukPkg.coverage.maxLatitude = 31.5;
            ukPkg.coverage.minLongitude = 77.5;
            ukPkg.coverage.maxLongitude = 81.1;

            if (!mapPath.isEmpty()) { ukPkg.mapFilePath = mapPath; ukPkg.hasRenderingData = true; }
            if (!graphPath.isEmpty()) { ukPkg.graphFilePath = graphPath; ukPkg.hasRoutingData = true; }
            if (!boundaryPath.isEmpty()) { ukPkg.boundaryFilePath = boundaryPath; }
            if (!placesPath.isEmpty()) { ukPkg.placesFilePath = placesPath; }

            if (m_packageManager->registerExistingPackage(ukPkg)) {
                m_packageManager->activateRegion(QStringLiteral("uttarakhand"));
                qInfo() << "[AppState] Registered in-situ development package for uttarakhand";
            }
        } else {
            qWarning() << "[AppState] No valid offline map package found. No active region.";
        }
    }

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

    if (m_locationService && m_emergencyService) {
        connect(m_locationService.get(), &georescux::domain::LocationService::positionChanged,
                this, [this](const georescux::domain::GeoPosition &pos) {
            if (m_emergencyService->hasActiveEmergency()) {
                const QString userId = m_session.currentUserId();
                const QString emergencyId = m_emergencyService->activeEmergencyId();
                if (!userId.isEmpty() && !emergencyId.isEmpty()) {
                    georescux::domain::EmergencyLocation loc;
                    loc.latitude = pos.latitude;
                    loc.longitude = pos.longitude;
                    loc.accuracyMeters = pos.accuracyMeters;
                    loc.timestampUtcMs = pos.timestampUtcMs;
                    m_emergencyService->updateLocation(userId, emergencyId, loc, georescux::domain::EmergencyLocationStatus::Available);
                }
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
            if (m_locationService) {
                m_locationService->setEmergencyMode(false);
                m_locationService->stopTracking();
            }
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
            if (m_locationService) {
                if (m_emergencyService && m_emergencyService->hasActiveEmergency()) {
                    m_locationService->setEmergencyMode(true);
                }
                m_locationService->startTracking();
            }
            m_currentNavigation = QStringLiteral("UserHome");
            emit navigationChanged(m_currentNavigation);
            break;

        case georescux::domain::SessionState::AuthenticatedAdmin:
            if (m_locationService) {
                m_locationService->setEmergencyMode(false);
            }
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
    if (m_session.isAdmin() || m_session.currentRole() != georescux::domain::UserRole::CitizenUser) {
        qWarning() << "[AppState] Authorization failure: Admin sessions are not permitted to activate citizen SOS.";
        return false;
    }
    if (!m_emergencyService) {
        return false;
    }

    const bool activated = m_emergencyService->activateEmergency(m_session.currentUserId(), note);
    if (activated && m_locationService) {
        // Switch location service to high-frequency emergency tracking mode (2s / 2m)
        m_locationService->setEmergencyMode(true);
        if (!m_locationService->isTracking()) {
            m_locationService->startTracking();
        }

        // Attach current location fix if already valid, or record permission/acquisition state
        const QString userId = m_session.currentUserId();
        const QString emergencyId = m_emergencyService->activeEmergencyId();
        if (m_locationService->hasValidFix()) {
            const auto &fix = m_locationService->lastValidPosition().value();
            georescux::domain::EmergencyLocation loc;
            loc.latitude = fix.latitude;
            loc.longitude = fix.longitude;
            loc.accuracyMeters = fix.accuracyMeters;
            loc.timestampUtcMs = fix.timestampUtcMs;
            m_emergencyService->updateLocation(userId, emergencyId, loc, georescux::domain::EmergencyLocationStatus::Available);
        } else if (m_locationService->permissionStatus() == georescux::domain::LocationPermissionStatus::Denied) {
            m_emergencyService->updateLocation(userId, emergencyId, {}, georescux::domain::EmergencyLocationStatus::PermissionDenied);
        } else {
            m_emergencyService->updateLocation(userId, emergencyId, {}, georescux::domain::EmergencyLocationStatus::Unavailable);
        }
    }
    return activated;
}

bool AppState::resolveSos() {
    if (!m_session.isAuthenticated() || m_session.currentUserId().isEmpty()) {
        qWarning() << "[AppState] Cannot resolve SOS: user is not authenticated.";
        return false;
    }
    if (m_session.isAdmin() || m_session.currentRole() != georescux::domain::UserRole::CitizenUser) {
        qWarning() << "[AppState] Authorization failure: Admin sessions are not permitted to resolve citizen SOS.";
        return false;
    }
    if (!m_emergencyService || !m_emergencyService->hasActiveEmergency()) {
        qWarning() << "[AppState] No active SOS to resolve.";
        return false;
    }

    const bool resolved = m_emergencyService->completeEmergency(m_session.currentUserId(), m_emergencyService->activeEmergencyId());
    if (resolved && m_locationService) {
        // Revert to normal tracking policy (5s / 5m)
        m_locationService->setEmergencyMode(false);
    }
    return resolved;
}

bool AppState::updateActiveNote(const QString &note) {
    if (!m_session.isAuthenticated() || m_session.currentUserId().isEmpty()) {
        qWarning() << "[AppState] Cannot update SOS note: user is not authenticated.";
        return false;
    }
    if (m_session.isAdmin() || m_session.currentRole() != georescux::domain::UserRole::CitizenUser) {
        qWarning() << "[AppState] Authorization failure: Admin sessions are not permitted to mutate citizen SOS.";
        return false;
    }
    if (!m_emergencyService || !m_emergencyService->hasActiveEmergency()) {
        qWarning() << "[AppState] No active SOS to update note.";
        return false;
    }
    return m_emergencyService->updateActiveNote(m_session.currentUserId(), m_emergencyService->activeEmergencyId(), note);
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
    if (m_locationService) {
        m_locationService->setEmergencyMode(false);
        m_locationService->stopTracking();
    }
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
            if (m_locationService) {
                if (m_emergencyService && m_emergencyService->hasActiveEmergency()) {
                    m_locationService->setEmergencyMode(true);
                }
                m_locationService->startTracking();
            }
            return true;
        }
    }
    return false;
}

QString AppState::activeRegionId() const {
    return m_packageManager ? m_packageManager->activeRegionId() : QString();
}

QString AppState::activeRegionDisplayName() const {
    return m_packageManager ? m_packageManager->activeRegionDisplayName() : QString();
}

bool AppState::hasActiveMapPackage() const {
    return m_packageManager && m_packageManager->hasActivePackage();
}

QString AppState::activeBoundaryPath() const {
    if (!m_packageManager) return {};
    auto pkg = m_packageManager->getActivePackage();
    return pkg ? pkg->boundaryFilePath : QString();
}

QString AppState::activeGraphPath() const {
    if (!m_packageManager) return {};
    auto pkg = m_packageManager->getActivePackage();
    return pkg ? pkg->graphFilePath : QString();
}

QString AppState::activePlacesPath() const {
    if (!m_packageManager) return {};
    auto pkg = m_packageManager->getActivePackage();
    return pkg ? pkg->placesFilePath : QString();
}

bool AppState::selectRegion(const QString &regionId) {
    if (!m_packageManager) return false;
    const bool ok = m_packageManager->activateRegion(regionId);
    if (ok) {
        if (m_routeService) {
            m_routeService->clearRoute();
        }
        emit activeRegionChanged(regionId);
    }
    return ok;
}

bool AppState::calculateRoute(double startLat, double startLon, double destLat, double destLon, bool avoidHazards) {
    if (!m_routeService) return false;
    const QString rId = activeRegionId();
    if (rId.isEmpty()) {
        qWarning() << "[AppState] Cannot calculate route: no active region selected.";
        return false;
    }
    return m_routeService->calculateRoute(rId, startLat, startLon, destLat, destLon, avoidHazards);
}

void AppState::clearRoute() {
    if (m_routeService) {
        m_routeService->clearRoute();
    }
}

} // namespace georescux::app
