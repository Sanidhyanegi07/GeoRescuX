#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QDebug>

#include "PrototypeBackend.h"

int main(int argc, char *argv[]) {
    // Set cross-platform Basic style for custom QML controls before QGuiApplication
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");

    QGuiApplication::setApplicationName(QStringLiteral("GeoRescuX Qt Prototype"));
    QGuiApplication::setOrganizationName(QStringLiteral("GeoRescuX"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QGuiApplication app(argc, argv);
    qInfo() << "[GeoRescuX] Starting Stage 1 Qt Prototype runtime. C++20 standard active.";

    const bool isSmokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
    if (isSmokeTest) {
        qInfo() << "[GeoRescuX Smoke Test] Automated validation mode active.";
    }

    // Backend declared before engine so engine is destroyed first upon exit
    PrototypeBackend backend(&app);
    QQmlApplicationEngine engine;

    // Connect application lifecycle state changes
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, [&backend](Qt::ApplicationState state) {
        QString stateName;
        switch (state) {
            case Qt::ApplicationActive: stateName = QStringLiteral("Active (Foreground)"); break;
            case Qt::ApplicationInactive: stateName = QStringLiteral("Inactive"); break;
            case Qt::ApplicationSuspended: stateName = QStringLiteral("Suspended (Background)"); break;
            case Qt::ApplicationHidden: stateName = QStringLiteral("Hidden"); break;
        }
        backend.notifyAppLifecycleState(stateName);
    });

    // Expose backend object to QML context
    engine.rootContext()->setContextProperty(QStringLiteral("prototypeBackend"), &backend);

    const QUrl url(QStringLiteral("qrc:/qt/qml/GeoRescuX/Prototype/ui/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, isSmokeTest, &backend](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "[GeoRescuX] Fatal: Failed to load root QML object.";
            QCoreApplication::exit(-1);
            return;
        }
        qInfo() << "[GeoRescuX] Root QML scene loaded successfully.";

        if (isSmokeTest) {
            // Verify QML-to-C++ interaction & signals
            QTimer::singleShot(250, [&backend]() {
                qInfo() << "[GeoRescuX Smoke Test] Testing QML-to-C++ invocation...";
                bool signalReceived = false;
                double receivedDistance = 0.0;

                QObject::connect(&backend, &PrototypeBackend::calculationFinished,
                                 [&signalReceived, &receivedDistance](double dist, const QString &details) {
                    signalReceived = true;
                    receivedDistance = dist;
                    qInfo().noquote() << "[GeoRescuX Smoke Test] Signal received successfully! Distance:" << dist << "m | Details:" << details;
                });

                // Simulate QML trigger
                backend.runGeodesicBenchmark();

                if (!signalReceived || receivedDistance <= 0.0) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Signal was not emitted or distance invalid.";
                    QCoreApplication::exit(1);
                } else if (backend.triggerCount() != 1) {
                    qCritical() << "[GeoRescuX Smoke Test] FAILED: Trigger count mismatch.";
                    QCoreApplication::exit(2);
                } else {
                    qInfo() << "[GeoRescuX Smoke Test] ALL CHECKS PASSED: QML scene loaded, C++ backend invoked, signal emitted & caught, distance validated.";
                    QCoreApplication::exit(0);
                }
            });
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
