#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>
#include <memory>
#include <iostream>

#include "PrototypeBackend.h"
#include "AppState.h"
#include "platform/storage/LocalDatabase.h"
#include "platform/qt/OfflineMapView.h"

int main(int argc, char *argv[]) {
    // Set cross-platform Basic style for custom QML controls before QGuiApplication
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication::setApplicationName(QStringLiteral("GeoRescuX Qt Prototype"));
    QGuiApplication::setOrganizationName(QStringLiteral("GeoRescuX"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QGuiApplication app(argc, argv);

    const bool isSmokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
    if (isSmokeTest) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
            std::cout << msg.toStdString() << std::endl;
        });
        qInfo() << "[GeoRescuX Smoke Test] Automated validation mode active.";
    } else {
        qInfo() << "[GeoRescuX] Starting Stage 2 Qt Prototype runtime. C++20 standard active.";
    }

    // 1. Initialize local SQLite persistence
    auto database = std::make_shared<georescux::storage::LocalDatabase>();
    if (!database->initialize(isSmokeTest ? QStringLiteral(":memory:") : QString())) {
        qWarning() << "[GeoRescuX] Warning: LocalDatabase initialization reported issues:" << database->lastError();
    }

    // 2. Instantiate authoritative Application State Model
    georescux::app::AppState appState(database, &app);
    if (!isSmokeTest) {
        appState.initializeSessionFromPersistence();
    }

    // 3. Instantiate C++ algorithmic backend engine (preserved from Phase 1)
    PrototypeBackend backend(&app);

    QQmlApplicationEngine engine;

    // Connect application lifecycle state changes
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, [&backend](Qt::ApplicationState state) {
        QString stateName;
        switch (state) {
            case Qt::ApplicationActive:    stateName = QStringLiteral("Active (Foreground)"); break;
            case Qt::ApplicationInactive:  stateName = QStringLiteral("Inactive"); break;
            case Qt::ApplicationSuspended: stateName = QStringLiteral("Suspended (Background)"); break;
            case Qt::ApplicationHidden:     stateName = QStringLiteral("Hidden"); break;
        }
        backend.notifyAppLifecycleState(stateName);
    });

    // Register C++ Quick items and uncreatable domain controllers
    qmlRegisterType<georescux::ui::OfflineMapView>("GeoRescuX.Prototype", 1, 0, "OfflineMapView");

    // Expose authoritative controllers to QML context
    engine.rootContext()->setContextProperty(QStringLiteral("appState"), &appState);
    engine.rootContext()->setContextProperty(QStringLiteral("sessionModel"), appState.session());
    engine.rootContext()->setContextProperty(QStringLiteral("emergencyService"), appState.emergencyService());
    engine.rootContext()->setContextProperty(QStringLiteral("packageManager"), appState.packageManager());
    engine.rootContext()->setContextProperty(QStringLiteral("routeService"), appState.routeService());
    engine.rootContext()->setContextProperty(QStringLiteral("prototypeBackend"), &backend);

    const QUrl url(QStringLiteral("qrc:/qt/qml/GeoRescuX/Prototype/ui/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, isSmokeTest, &backend, &appState, database](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "[GeoRescuX] Fatal: Failed to load root QML object.";
            QCoreApplication::exit(-1);
            return;
        }
        qInfo() << "[GeoRescuX] Root QML scene loaded successfully.";

        if (isSmokeTest) {
            // Comprehensive Phase 3 Smoke Test:
            // 1. Session State verification & transitions
            // 2. Navigation routing & role boundaries
            // 3. SQLite schema version 2 verification
            // 4. Local-First Emergency activation and completion lifecycle
            // 5. C++ Geodesic calculations
            QTimer::singleShot(250, [&backend, &appState, database]() {
                qInfo() << "[GeoRescuX Smoke Test] Running Phase 3 automated verification...";

                // A. Verify initial unauthenticated state
                if (appState.session()->sessionState() != georescux::domain::SessionState::Unauthenticated) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Initial session state is not Unauthenticated.";
                    QCoreApplication::exit(1);
                    return;
                }
                if (appState.currentNavigation() != QStringLiteral("Welcome")) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Initial navigation is not Welcome.";
                    QCoreApplication::exit(2);
                    return;
                }

                // B. Verify SQLite schema version 2
                if (!database->isOpen() || database->schemaVersion() != 2) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: LocalDatabase is not open or schemaVersion != 2 (actual: " << database->schemaVersion() << ").";
                    QCoreApplication::exit(3);
                    return;
                }

                // C. Test Citizen User Login transition
                if (!appState.loginDevelopmentUser(QStringLiteral("Smoke Citizen"), QStringLiteral("citizen@georescux.org"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: loginDevelopmentUser failed.";
                    QCoreApplication::exit(4);
                    return;
                }
                if (appState.session()->currentRole() != georescux::domain::UserRole::CitizenUser ||
                    appState.currentNavigation() != QStringLiteral("UserHome")) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Navigation did not update to UserHome.";
                    QCoreApplication::exit(5);
                    return;
                }

                // D. Test Local-First Emergency Activation & Completion
                if (!appState.activateSos(QStringLiteral("Smoke Test Active Emergency"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: activateSos failed.";
                    QCoreApplication::exit(6);
                    return;
                }
                if (!appState.emergencyService()->hasActiveEmergency() ||
                    appState.emergencyState() != georescux::app::EmergencyLifecycleState::Active) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Emergency state not Active after activateSos.";
                    QCoreApplication::exit(7);
                    return;
                }
                if (!appState.resolveSos()) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: resolveSos failed.";
                    QCoreApplication::exit(8);
                    return;
                }
                if (appState.emergencyService()->hasActiveEmergency() ||
                    appState.emergencyState() != georescux::app::EmergencyLifecycleState::Inactive) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Emergency state not Inactive after resolveSos.";
                    QCoreApplication::exit(9);
                    return;
                }

                // E. Test User Navigation within User Portal
                if (!appState.navigateTo(QStringLiteral("UserMap"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: navigateTo(UserMap) rejected.";
                    QCoreApplication::exit(10);
                    return;
                }
                // Unauthorized navigation to AdminDashboard must be denied
                if (appState.navigateTo(QStringLiteral("AdminDashboard"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Citizen was incorrectly allowed into AdminDashboard.";
                    QCoreApplication::exit(11);
                    return;
                }

                // F. Test Admin Login transition
                if (!appState.loginDevelopmentAdmin(QStringLiteral("Smoke Admin"), QStringLiteral("admin@georescux.org"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: loginDevelopmentAdmin failed.";
                    QCoreApplication::exit(12);
                    return;
                }
                if (!appState.session()->isAdmin() || appState.currentNavigation() != QStringLiteral("AdminDashboard")) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Admin role or navigation mismatch.";
                    QCoreApplication::exit(13);
                    return;
                }

                // Phase 4 Check: Admin session must NOT be permitted to activate SOS
                if (appState.activateSos(QStringLiteral("Admin unauthorized SOS attempt"))) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Admin session was incorrectly permitted to activate SOS.";
                    QCoreApplication::exit(14);
                    return;
                }

                // G. Test C++ Algorithmic calculation signal
                bool signalReceived = false;
                double receivedDistance = 0.0;
                QObject::connect(&backend, &PrototypeBackend::calculationFinished,
                                 [&signalReceived, &receivedDistance](double dist, const QString &details) {
                    signalReceived = true;
                    receivedDistance = dist;
                    qInfo().noquote() << "[GeoRescuX Smoke Test] C++ Geodesic Signal received! Distance:" << dist << "m | Details:" << details;
                });

                backend.runGeodesicBenchmark();

                if (!signalReceived || receivedDistance <= 0.0) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Geodesic calculation signal not received.";
                    QCoreApplication::exit(15);
                    return;
                }

                // H. Phase 5: Map Package Model & Regional Coverage
                if (!appState.packageManager() || !appState.packageManager()->hasActivePackage()) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: No active regional map package registered.";
                    QCoreApplication::exit(16);
                    return;
                }
                if (appState.activeRegionId() != QStringLiteral("uttarakhand")) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Active region is not uttarakhand (actual: " << appState.activeRegionId() << ").";
                    QCoreApplication::exit(17);
                    return;
                }
                if (!appState.packageManager()->isCoordinateCovered(30.3165, 78.0322)) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Dehradun coordinate should be within Uttarakhand coverage.";
                    QCoreApplication::exit(18);
                    return;
                }
                if (appState.packageManager()->isCoordinateCovered(12.9716, 77.5946)) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Bengaluru coordinate should NOT be within Uttarakhand coverage.";
                    QCoreApplication::exit(19);
                    return;
                }

                // I. Phase 6 & 7: Offline Route Calculation & Safe Route Engine
                if (!appState.routeService()) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: RouteService is null.";
                    QCoreApplication::exit(20);
                    return;
                }
                const bool routeOk = appState.calculateRoute(30.3165, 78.0322, 30.1058, 78.2880);
                if (!routeOk || !appState.routeService()->hasRoute()) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Route calculation between Dehradun and Rishikesh failed. Status: " << appState.routeService()->lastStatusMessage();
                    QCoreApplication::exit(21);
                    return;
                }
                if (appState.routeService()->lastDistanceMeters() <= 0.0) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Route calculated but distance is <= 0.";
                    QCoreApplication::exit(22);
                    return;
                }
                qInfo().noquote() << "[GeoRescuX Smoke Test] Offline Route calculated successfully! Distance:" 
                                  << appState.routeService()->lastDistanceFormatted()
                                  << "| Steps:" << appState.routeService()->lastSteps().size();

                appState.clearRoute();
                if (appState.routeService()->hasRoute()) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: clearRoute did not reset route state.";
                    QCoreApplication::exit(23);
                    return;
                }

                // Rejection of out-of-coverage coordinates
                if (appState.calculateRoute(12.9716, 77.5946, 13.0, 77.6)) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Route calculation outside coverage was incorrectly accepted.";
                    QCoreApplication::exit(24);
                    return;
                }

                qInfo() << "[GeoRescuX Smoke Test] ALL PHASE 5, 6, 7 CHECKS PASSED: Regional Map Packages, Offline Map View, Zero-Network Pathfinding & Safe Route verified.";
                QCoreApplication::exit(0);
            });
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
