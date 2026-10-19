#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QDebug>

#include "PrototypeBackend.h"

int main(int argc, char *argv[]) {
    // Android scaling & high-DPI
    QGuiApplication::setApplicationName(QStringLiteral("GeoRescuX Qt Prototype"));
    QGuiApplication::setOrganizationName(QStringLiteral("GeoRescuX"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QGuiApplication app(argc, argv);
    qInfo() << "[GeoRescuX] Starting Stage 1 Qt Prototype runtime. C++20 standard active.";

    QQmlApplicationEngine engine;
    auto backend = std::make_unique<PrototypeBackend>();

    // Connect application lifecycle state changes
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, [&backend](Qt::ApplicationState state) {
        QString stateName;
        switch (state) {
            case Qt::ApplicationActive: stateName = QStringLiteral("Active (Foreground)"); break;
            case Qt::ApplicationInactive: stateName = QStringLiteral("Inactive"); break;
            case Qt::ApplicationSuspended: stateName = QStringLiteral("Suspended (Background)"); break;
            case Qt::ApplicationHidden: stateName = QStringLiteral("Hidden"); break;
        }
        backend->notifyAppLifecycleState(stateName);
    });

    // Expose backend object to QML context
    engine.rootContext()->setContextProperty(QStringLiteral("prototypeBackend"), backend.get());

    const QUrl url(QStringLiteral("qrc:/GeoRescuX/Prototype/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "[GeoRescuX] Fatal: Failed to load root QML object.";
            QCoreApplication::exit(-1);
        } else {
            qInfo() << "[GeoRescuX] Root QML scene loaded successfully.";
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
