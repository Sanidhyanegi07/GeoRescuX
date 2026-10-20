#include "OfflineRouteRepository.h"
#include "GraphJsonParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

namespace georescux::storage {

OfflineRouteRepository::OfflineRouteRepository(std::shared_ptr<georescux::domain::MapPackageManager> packageManager)
    : m_packageManager(std::move(packageManager)) {
}

QString OfflineRouteRepository::resolveGraphFilePath(const QString &regionId) const {
    const std::string ridStd = regionId.toStdString();
    auto itCustom = m_customPaths.find(ridStd);
    if (itCustom != m_customPaths.end() && QFile::exists(QString::fromStdString(itCustom->second))) {
        return QString::fromStdString(itCustom->second);
    }

    if (m_packageManager) {
        auto pkg = m_packageManager->getPackage(regionId);
        if (pkg && !pkg->graphFilePath.isEmpty() && QFile::exists(pkg->graphFilePath)) {
            return pkg->graphFilePath;
        }
    }

    // Check candidate project paths
    const QStringList baseNames = {
        QStringLiteral("map_packages/%1/route_graph.json").arg(regionId),
        QStringLiteral("prototype_qt/platform/android/assets/map_packages/%1/route_graph.json").arg(regionId),
        QStringLiteral("app/src/main/assets/route_graph_%1").arg(regionId),
        QStringLiteral("app/src/main/assets/maps/%1/graph.json").arg(regionId)
    };

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList prefixes = {
        QStringLiteral(""),
        QStringLiteral("../"),
        QStringLiteral("../../"),
        QStringLiteral("../../../")
    };

    for (const auto &base : baseNames) {
        for (const auto &pfx : prefixes) {
            const QString cand = pfx + base;
            if (QFile::exists(cand)) {
                return cand;
            }
            if (!appDir.isEmpty()) {
                const QString candApp = QDir(appDir).filePath(cand);
                if (QFile::exists(candApp)) {
                    return candApp;
                }
            }
        }
        const QString androidAsset = QStringLiteral("assets:/") + base;
        if (QFile::exists(androidAsset)) {
            return androidAsset;
        }
    }
    return QString();
}

void OfflineRouteRepository::setCustomGraphPath(const QString &regionId, const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    m_customPaths[regionId.toStdString()] = filePath.toStdString();
}

void OfflineRouteRepository::seedGraph(const QString &regionId, const georescux::routing::RouteGraph &graph) {
    QMutexLocker locker(&m_mutex);
    m_cache[regionId.toStdString()] = graph;
}

bool OfflineRouteRepository::hasGraph(const QString &regionId) {
    QMutexLocker locker(&m_mutex);
    const std::string rid = regionId.toStdString();
    if (m_cache.find(rid) != m_cache.end()) {
        return true;
    }
    const QString path = resolveGraphFilePath(regionId);
    return !path.isEmpty();
}

std::optional<georescux::routing::RouteGraph> OfflineRouteRepository::getGraph(const QString &regionId) {
    QMutexLocker locker(&m_mutex);
    const std::string rid = regionId.toStdString();
    auto it = m_cache.find(rid);
    if (it != m_cache.end()) {
        return it->second;
    }

    const QString filePath = resolveGraphFilePath(regionId);
    if (filePath.isEmpty()) {
        qWarning() << "[OfflineRouteRepository] No graph file found for region:" << regionId;
        return std::nullopt;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[OfflineRouteRepository] Failed to open graph file:" << filePath;
        return std::nullopt;
    }

    const QByteArray data = file.readAll();
    file.close();

    const std::string jsonStr = data.toStdString();
    auto graphOpt = georescux::routing::GraphJsonParser::parse(jsonStr);
    if (!graphOpt) {
        qWarning() << "[OfflineRouteRepository] Failed to parse graph JSON for region:" << regionId;
        return std::nullopt;
    }

    m_cache[rid] = *graphOpt;
    qInfo() << "[OfflineRouteRepository] Loaded and memoized graph for" << regionId
            << "with" << graphOpt->nodes.size() << "nodes and" << graphOpt->edges.size() << "edges.";
    return graphOpt;
}

bool OfflineRouteRepository::saveGraph(const QString &regionId, const georescux::routing::RouteGraph &graph) {
    QMutexLocker locker(&m_mutex);
    m_cache[regionId.toStdString()] = graph;
    return true;
}

void OfflineRouteRepository::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

} // namespace georescux::storage
