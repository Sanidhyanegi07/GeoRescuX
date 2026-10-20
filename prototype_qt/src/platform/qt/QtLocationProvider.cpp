#include "QtLocationProvider.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QPermissions>

#if defined(Q_OS_ANDROID)
#include <QtCore/qnativeinterface.h>
#include <QJniObject>
#include <QJniEnvironment>
#endif

namespace georescux::platform {

QtLocationProvider::QtLocationProvider(QObject *parent)
    : QObject(parent) {
    s_activeInstance = this;
    connect(&m_pollTimer, &QTimer::timeout, this, &QtLocationProvider::handlePollingTick);
    querySystemPermission();
}

QtLocationProvider::~QtLocationProvider() {
    stopUpdates();
    if (s_activeInstance == this) {
        s_activeInstance = nullptr;
    }
}

georescux::domain::LocationPermissionStatus QtLocationProvider::permissionStatus() const {
    return m_permission;
}

void QtLocationProvider::querySystemPermission() {
    QLocationPermission perm;
    perm.setAccuracy(QLocationPermission::Precise);
    perm.setAvailability(QLocationPermission::WhenInUse);

    const Qt::PermissionStatus status = qApp ? qApp->checkPermission(perm) : Qt::PermissionStatus::Undetermined;
    switch (status) {
        case Qt::PermissionStatus::Granted:
            m_permission = georescux::domain::LocationPermissionStatus::Granted;
            break;
        case Qt::PermissionStatus::Denied:
            m_permission = georescux::domain::LocationPermissionStatus::Denied;
            break;
        case Qt::PermissionStatus::Undetermined:
        default:
            m_permission = georescux::domain::LocationPermissionStatus::NotRequested;
            break;
    }
}

void QtLocationProvider::requestPermission() {
    if (!qApp) {
        return;
    }

    QLocationPermission perm;
    perm.setAccuracy(QLocationPermission::Precise);
    perm.setAvailability(QLocationPermission::WhenInUse);

    qApp->requestPermission(perm, [this](const QPermission &p) {
        const auto status = p.status();
        if (status == Qt::PermissionStatus::Granted) {
            m_permission = georescux::domain::LocationPermissionStatus::Granted;
            qInfo() << "[QtLocationProvider] Location permission GRANTED by system.";
        } else if (status == Qt::PermissionStatus::Denied) {
            m_permission = georescux::domain::LocationPermissionStatus::Denied;
            qWarning() << "[QtLocationProvider] Location permission DENIED by system.";
        } else {
            m_permission = georescux::domain::LocationPermissionStatus::NotRequested;
        }

        if (m_onPermissionChanged) {
            m_onPermissionChanged(m_permission);
        }

        if (m_isUpdating) {
            if (m_permission == georescux::domain::LocationPermissionStatus::Denied) {
                if (m_onStateChanged) {
                    m_onStateChanged(georescux::domain::LocationTrackingState::PermissionDenied);
                }
            } else if (m_permission == georescux::domain::LocationPermissionStatus::Granted) {
                if (m_onStateChanged) {
                    m_onStateChanged(georescux::domain::LocationTrackingState::AcquisitionPending);
                }
            }
        }
    });
}

bool QtLocationProvider::isLocationServicesEnabled() const {
#if defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject bridge = QJniObject::callStaticObjectMethod(
            "com/example/georescux/qtprototype/AndroidLocationBridge",
            "getInstance",
            "(Landroid/content/Context;)Lcom/example/georescux/qtprototype/AndroidLocationBridge;",
            context.object<jobject>()
        );
        if (bridge.isValid()) {
            return bridge.callMethod<jboolean>("isLocationServicesEnabled", "()Z");
        }
    }
#endif
    return m_servicesEnabled;
}

void QtLocationProvider::setLocationServicesEnabled(bool enabled) {
    m_servicesEnabled = enabled;
    if (!m_servicesEnabled && m_isUpdating && m_onStateChanged) {
        m_onStateChanged(georescux::domain::LocationTrackingState::LocationServicesDisabled);
    }
}

void QtLocationProvider::startUpdates(bool highFrequencyEmergency) {
    m_isUpdating = true;
    m_isEmergencyFrequency = highFrequencyEmergency;

    querySystemPermission();

    if (m_permission == georescux::domain::LocationPermissionStatus::Denied) {
        if (m_onStateChanged) {
            m_onStateChanged(georescux::domain::LocationTrackingState::PermissionDenied);
        }
        return;
    }

    if (!isLocationServicesEnabled()) {
        if (m_onStateChanged) {
            m_onStateChanged(georescux::domain::LocationTrackingState::LocationServicesDisabled);
        }
        return;
    }

    if (m_onStateChanged) {
        m_onStateChanged(georescux::domain::LocationTrackingState::AcquisitionPending);
    }

    const int intervalMs = highFrequencyEmergency ? 2000 : 5000;
    const double minDistanceMeters = highFrequencyEmergency ? 2.0 : 5.0;

#if defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject bridge = QJniObject::callStaticObjectMethod(
            "com/example/georescux/qtprototype/AndroidLocationBridge",
            "getInstance",
            "(Landroid/content/Context;)Lcom/example/georescux/qtprototype/AndroidLocationBridge;",
            context.object<jobject>()
        );
        if (bridge.isValid()) {
            bridge.callMethod<void>(
                "startLocationUpdates",
                "(JF)V",
                static_cast<jlong>(intervalMs),
                static_cast<jfloat>(minDistanceMeters)
            );
            qInfo() << "[QtLocationProvider] Started Android hardware LocationManager updates (interval:"
                    << intervalMs << "ms, minDistance:" << minDistanceMeters << "m)";
        } else {
            qWarning() << "[QtLocationProvider] AndroidLocationBridge instance unavailable via JNI.";
        }
    }
#endif

    m_pollTimer.setInterval(intervalMs);
    m_pollTimer.start();

    // Trigger immediate poll on start for desktop simulation / cache
    handlePollingTick();
}

void QtLocationProvider::stopUpdates() {
    m_isUpdating = false;
    m_isEmergencyFrequency = false;
    m_pollTimer.stop();

#if defined(Q_OS_ANDROID)
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject bridge = QJniObject::callStaticObjectMethod(
            "com/example/georescux/qtprototype/AndroidLocationBridge",
            "getInstance",
            "(Landroid/content/Context;)Lcom/example/georescux/qtprototype/AndroidLocationBridge;",
            context.object<jobject>()
        );
        if (bridge.isValid()) {
            bridge.callMethod<void>("stopLocationUpdates", "()V");
            qInfo() << "[QtLocationProvider] Stopped Android hardware LocationManager updates.";
        }
    }
#endif

    if (m_onStateChanged) {
        m_onStateChanged(georescux::domain::LocationTrackingState::Idle);
    }
}

bool QtLocationProvider::isUpdating() const {
    return m_isUpdating;
}

void QtLocationProvider::setOnPositionChanged(std::function<void(const georescux::domain::GeoPosition&)> callback) {
    m_onPositionChanged = std::move(callback);
}

void QtLocationProvider::setOnStateChanged(std::function<void(georescux::domain::LocationTrackingState)> callback) {
    m_onStateChanged = std::move(callback);
}

void QtLocationProvider::setOnPermissionChanged(std::function<void(georescux::domain::LocationPermissionStatus)> callback) {
    m_onPermissionChanged = std::move(callback);
}

void QtLocationProvider::setSimulatedPosition(double lat, double lon, double accuracyMeters) {
    m_hasSimulatedPos = true;
    m_simLat = lat;
    m_simLon = lon;
    m_simAcc = accuracyMeters;

    if (m_isUpdating) {
        handlePollingTick();
    }
}

void QtLocationProvider::handleAndroidLocation(
    double lat, double lon, double alt, float acc, float speed, qint64 timestampUtcMs
) {
    // Thread safety: forward to provider's QObject event loop thread
    QMetaObject::invokeMethod(this, [this, lat, lon, alt, acc, speed, timestampUtcMs]() {
        if (!m_isUpdating) {
            return;
        }

        georescux::domain::GeoPosition pos{
            .latitude = lat,
            .longitude = lon,
            .altitudeMeters = alt,
            .accuracyMeters = static_cast<double>(acc),
            .speedMps = static_cast<double>(speed),
            .timestampUtcMs = timestampUtcMs > 0 ? timestampUtcMs : QDateTime::currentMSecsSinceEpoch(),
            .isValidCoordinates = true
        };

        if (m_onPositionChanged) {
            m_onPositionChanged(pos);
        }
    }, Qt::QueuedConnection);
}

void QtLocationProvider::handleAndroidState(int stateCode) {
    QMetaObject::invokeMethod(this, [this, stateCode]() {
        auto state = static_cast<georescux::domain::LocationTrackingState>(stateCode);
        if (m_onStateChanged) {
            m_onStateChanged(state);
        }
    }, Qt::QueuedConnection);
}

void QtLocationProvider::handlePollingTick() {
    if (!m_isUpdating) {
        return;
    }

    if (m_permission == georescux::domain::LocationPermissionStatus::Denied) {
        if (m_onStateChanged) {
            m_onStateChanged(georescux::domain::LocationTrackingState::PermissionDenied);
        }
        return;
    }

    if (!isLocationServicesEnabled()) {
        if (m_onStateChanged) {
            m_onStateChanged(georescux::domain::LocationTrackingState::LocationServicesDisabled);
        }
        return;
    }

    if (m_hasSimulatedPos) {
        georescux::domain::GeoPosition pos{
            .latitude = m_simLat,
            .longitude = m_simLon,
            .altitudeMeters = 640.0,
            .accuracyMeters = m_simAcc,
            .speedMps = 0.0,
            .timestampUtcMs = QDateTime::currentMSecsSinceEpoch(),
            .isValidCoordinates = true
        };

        if (m_onPositionChanged) {
            m_onPositionChanged(pos);
        }
    }
}

} // namespace georescux::platform

#if defined(Q_OS_ANDROID)
extern "C" {

JNIEXPORT void JNICALL Java_com_example_georescux_qtprototype_AndroidLocationBridge_onNativeLocationChanged(
    JNIEnv *env,
    jclass clazz,
    jdouble lat,
    jdouble lon,
    jdouble altitude,
    jfloat accuracy,
    jfloat speed,
    jlong timestampUtcMs
) {
    Q_UNUSED(env);
    Q_UNUSED(clazz);
    if (auto *provider = georescux::platform::QtLocationProvider::activeInstance()) {
        provider->handleAndroidLocation(lat, lon, altitude, accuracy, speed, timestampUtcMs);
    }
}

JNIEXPORT void JNICALL Java_com_example_georescux_qtprototype_AndroidLocationBridge_onNativeStateChanged(
    JNIEnv *env,
    jclass clazz,
    jint stateCode
) {
    Q_UNUSED(env);
    Q_UNUSED(clazz);
    if (auto *provider = georescux::platform::QtLocationProvider::activeInstance()) {
        provider->handleAndroidState(stateCode);
    }
}

} // extern "C"
#endif
