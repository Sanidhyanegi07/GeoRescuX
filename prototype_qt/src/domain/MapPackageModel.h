#ifndef GEORESCUX_MAP_PACKAGE_MODEL_H
#define GEORESCUX_MAP_PACKAGE_MODEL_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <vector>
#include <optional>

namespace georescux::domain {

/**
 * @brief Lifecycle status of a regional map package.
 */
enum class PackageStatus {
    Discovered = 0,
    Validating = 1,
    Installed = 2,
    Active = 3,
    Corrupt = 4,
    Incompatible = 5,
    Missing = 6
};

inline QString packageStatusToString(PackageStatus status) {
    switch (status) {
        case PackageStatus::Discovered: return QStringLiteral("DISCOVERED");
        case PackageStatus::Validating: return QStringLiteral("VALIDATING");
        case PackageStatus::Installed: return QStringLiteral("INSTALLED");
        case PackageStatus::Active: return QStringLiteral("ACTIVE");
        case PackageStatus::Corrupt: return QStringLiteral("CORRUPT");
        case PackageStatus::Incompatible: return QStringLiteral("INCOMPATIBLE");
        case PackageStatus::Missing: return QStringLiteral("MISSING");
    }
    return QStringLiteral("UNKNOWN");
}

inline PackageStatus stringToPackageStatus(const QString &str) {
    const QString upper = str.trimmed().toUpper();
    if (upper == QStringLiteral("DISCOVERED")) return PackageStatus::Discovered;
    if (upper == QStringLiteral("VALIDATING")) return PackageStatus::Validating;
    if (upper == QStringLiteral("INSTALLED")) return PackageStatus::Installed;
    if (upper == QStringLiteral("ACTIVE")) return PackageStatus::Active;
    if (upper == QStringLiteral("CORRUPT")) return PackageStatus::Corrupt;
    if (upper == QStringLiteral("INCOMPATIBLE")) return PackageStatus::Incompatible;
    return PackageStatus::Missing;
}

/**
 * @brief Geographic bounding box for region coverage.
 */
struct GeographicCoverage {
    double minLatitude{0.0};
    double maxLatitude{0.0};
    double minLongitude{0.0};
    double maxLongitude{0.0};

    [[nodiscard]] bool isValid() const noexcept {
        return minLatitude >= -90.0 && maxLatitude <= 90.0 &&
               minLatitude < maxLatitude &&
               minLongitude >= -180.0 && maxLongitude <= 180.0 &&
               minLongitude < maxLongitude;
    }

    [[nodiscard]] bool contains(double lat, double lon) const noexcept {
        if (!isValid()) return false;
        return lat >= minLatitude && lat <= maxLatitude &&
               lon >= minLongitude && lon <= maxLongitude;
    }
};

/**
 * @brief Immutable specification and status of an offline regional map package.
 */
struct MapPackage {
    QString regionId;
    QString displayName;
    int packageVersion{1};
    QString supportedFormat{QStringLiteral("Mapsforge MAP")};
    GeographicCoverage coverage;
    
    // Core data file paths (relative to package root or absolute)
    QString mapFilePath;
    QString graphFilePath;
    QString placesFilePath;
    QString boundaryFilePath;

    qint64 fileSizeBytes{0};
    QString sha256Checksum;
    int routingDataVersion{1};
    qint64 installedAtUtcMs{0};
    PackageStatus status{PackageStatus::Discovered};
    QStringList validationErrors;

    bool hasRenderingData{false};
    bool hasRoutingData{false};

    [[nodiscard]] bool isValid() const noexcept {
        return !regionId.trimmed().isEmpty() &&
               !displayName.trimmed().isEmpty() &&
               packageVersion > 0 &&
               coverage.isValid() &&
               validationErrors.isEmpty();
    }

    [[nodiscard]] bool isCoordinateInside(double lat, double lon) const noexcept {
        return coverage.contains(lat, lon);
    }
};

} // namespace georescux::domain

#endif // GEORESCUX_MAP_PACKAGE_MODEL_H
