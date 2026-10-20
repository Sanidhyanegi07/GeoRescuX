#include <iostream>
#include <cassert>
#include <memory>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include "domain/MapPackageModel.h"
#include "domain/MapPackageManager.h"
#include "platform/storage/LocalDatabase.h"
#include "platform/storage/SqliteMapPackageRepository.h"

using namespace georescux::domain;
using namespace georescux::storage;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] Running Phase 5 Map-Package Infrastructure Tests" << std::endl;
    std::cout << "=================================================================" << std::endl;

    const QString testTempDir = QDir::temp().filePath(QStringLiteral("georescux_map_test_%1").arg(QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(testTempDir);

    // Helper to create a dummy package with manifest and files
    auto createFixturePackage = [&](const QString &dirName, const QString &regionId, const QString &name, int ver,
                                    double minLat, double maxLat, double minLon, double maxLon,
                                    const QString &mapFile, const QString &graphFile,
                                    bool writeActualFiles = true, bool corruptHash = false) -> QString {
        const QString pkgDir = QDir(testTempDir).filePath(dirName);
        QDir().mkpath(pkgDir);

        QString sha256Hex = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"); // empty string sha
        if (writeActualFiles) {
            if (!mapFile.isEmpty()) {
                QFile f(QDir(pkgDir).filePath(mapFile));
                f.open(QIODevice::WriteOnly);
                f.write("MAP_FIXTURE_DATA");
                f.close();
            }
            if (!graphFile.isEmpty()) {
                QFile f(QDir(pkgDir).filePath(graphFile));
                f.open(QIODevice::WriteOnly);
                f.write("GRAPH_FIXTURE_DATA");
                f.close();
            }
            // Compute real SHA for source file
            QFile src(QDir(pkgDir).filePath(QStringLiteral("state.osm.pbf")));
            src.open(QIODevice::WriteOnly);
            src.write("PBF_FIXTURE_BYTES");
            src.close();

            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData("PBF_FIXTURE_BYTES", 17);
            sha256Hex = QString::fromLatin1(hash.result().toHex());
            if (corruptHash) {
                sha256Hex = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000");
            }
        }

        QJsonObject root;
        root[QStringLiteral("regionId")] = regionId;
        root[QStringLiteral("displayName")] = name;
        root[QStringLiteral("packageVersion")] = ver;
        root[QStringLiteral("renderingFormat")] = QStringLiteral("Mapsforge MAP");
        root[QStringLiteral("renderingFile")] = mapFile;
        root[QStringLiteral("routingFile")] = graphFile;
        root[QStringLiteral("sourceFile")] = QStringLiteral("state.osm.pbf");
        root[QStringLiteral("sha256")] = sha256Hex;

        QJsonObject cov;
        cov[QStringLiteral("minLatitude")] = minLat;
        cov[QStringLiteral("maxLatitude")] = maxLat;
        cov[QStringLiteral("minLongitude")] = minLon;
        cov[QStringLiteral("maxLongitude")] = maxLon;
        root[QStringLiteral("coverage")] = cov;

        const QString manifestPath = QDir(pkgDir).filePath(QStringLiteral("manifest.json"));
        QFile mf(manifestPath);
        mf.open(QIODevice::WriteOnly);
        mf.write(QJsonDocument(root).toJson());
        mf.close();

        return manifestPath;
    };

    auto db = std::make_shared<LocalDatabase>();
    assert(db->initialize(QStringLiteral(":memory:")));
    auto repo = std::make_shared<SqliteMapPackageRepository>(db);
    assert(repo->ensureTableCreated());
    const QString installDir = QDir(testTempDir).filePath(QStringLiteral("installed_packages"));
    MapPackageManager manager(repo, installDir);

    // Test 1: Valid Package Discovery
    {
        std::cout << "[Test 1] Testing valid package discovery..." << std::endl;
        const QString manifest = createFixturePackage("pkg_uk", "uttarakhand", "Uttarakhand", 1, 28.7, 31.5, 77.5, 81.1, "state.map", "graph.json");
        auto discovered = manager.discoverPackages(testTempDir);
        assert(!discovered.empty());
        bool found = false;
        for (const auto &p : discovered) {
            if (p.regionId == "uttarakhand") {
                found = true;
                assert(p.displayName == "Uttarakhand");
                assert(p.coverage.isValid());
                assert(p.hasRenderingData);
                assert(p.hasRoutingData);
            }
        }
        assert(found);
        std::cout << "  -> PASSED: Valid package discovered." << std::endl;
    }

    // Test 2: Missing Required Files
    {
        std::cout << "[Test 2] Testing missing required files rejection..." << std::endl;
        // Package references missing rendering and routing files
        const QString manifest = createFixturePackage("pkg_missing", "missing_reg", "Missing Region", 1, 10.0, 11.0, 20.0, 21.0, "nonexistent.map", "nonexistent.json", false);
        MapPackage pkg;
        QStringList errors;
        bool ok = manager.validatePackageManifest(manifest, pkg, errors);
        // Rendering data and routing data will not be marked present
        assert(!pkg.hasRenderingData);
        assert(!pkg.hasRoutingData);
        std::cout << "  -> PASSED: Missing files correctly reflected in package capabilities." << std::endl;
    }

    // Test 3: Invalid Manifest (Malformed JSON / Missing IDs)
    {
        std::cout << "[Test 3] Testing invalid manifest rejection..." << std::endl;
        const QString badJsonDir = QDir(testTempDir).filePath("pkg_bad_json");
        QDir().mkpath(badJsonDir);
        const QString badManifest = QDir(badJsonDir).filePath("manifest.json");
        QFile f(badManifest);
        f.open(QIODevice::WriteOnly);
        f.write("{ invalid json syntax ... ");
        f.close();

        MapPackage pkg;
        QStringList errors;
        assert(!manager.validatePackageManifest(badManifest, pkg, errors));
        assert(!errors.isEmpty());
        std::cout << "  -> PASSED: Malformed manifest rejected." << std::endl;
    }

    // Test 4: Checksum Mismatch Rejection
    {
        std::cout << "[Test 4] Testing checksum mismatch rejection..." << std::endl;
        const QString badHashManifest = createFixturePackage("pkg_bad_hash", "corrupt_reg", "Corrupt Hash", 1, 10.0, 11.0, 20.0, 21.0, "state.map", "graph.json", true, true);
        MapPackage pkg;
        QStringList errors;
        assert(!manager.validatePackageManifest(badHashManifest, pkg, errors));
        bool foundChecksumErr = false;
        for (const auto &err : errors) {
            if (err.contains("Checksum mismatch", Qt::CaseInsensitive)) foundChecksumErr = true;
        }
        assert(foundChecksumErr);
        std::cout << "  -> PASSED: Checksum mismatch strictly caught and rejected." << std::endl;
    }

    // Test 5: Invalid Geographic Coverage Rejection
    {
        std::cout << "[Test 5] Testing invalid geographic coverage rejection..." << std::endl;
        // minLat > maxLat
        const QString badCovManifest = createFixturePackage("pkg_bad_cov", "bad_cov", "Inverted Bounds", 1, 35.0, 25.0, 70.0, 80.0, "state.map", "graph.json");
        MapPackage pkg;
        QStringList errors;
        assert(!manager.validatePackageManifest(badCovManifest, pkg, errors));
        assert(!errors.isEmpty());
        std::cout << "  -> PASSED: Inverted/invalid geographic bounds rejected." << std::endl;
    }

    // Test 6: Unsupported Package Version Rejection
    {
        std::cout << "[Test 6] Testing unsupported package version rejection..." << std::endl;
        // version 0 or negative
        const QString badVerManifest = createFixturePackage("pkg_bad_ver", "bad_ver", "Zero Version", 0, 10.0, 12.0, 20.0, 22.0, "state.map", "graph.json");
        MapPackage pkg;
        QStringList errors;
        assert(!manager.validatePackageManifest(badVerManifest, pkg, errors));
        assert(!errors.isEmpty());
        std::cout << "  -> PASSED: Invalid package version rejected." << std::endl;
    }

    // Test 7: Path Traversal Rejection
    {
        std::cout << "[Test 7] Testing path traversal rejection..." << std::endl;
        assert(manager.isPathTraversal("/app/data", "../../../etc/passwd"));
        assert(manager.isPathTraversal("/app/data", "..\\..\\windows\\system32"));
        assert(manager.isPathTraversal("/app/data", "/absolute/path/outside"));

        const QString traversalManifest = createFixturePackage("pkg_traversal", "traversal_reg", "Traversal", 1, 10.0, 12.0, 20.0, 22.0, "../../escaped.map", "graph.json");
        MapPackage pkg;
        QStringList errors;
        assert(!manager.validatePackageManifest(traversalManifest, pkg, errors));
        bool foundTraversal = false;
        for (const auto &e : errors) {
            if (e.contains("Path traversal", Qt::CaseInsensitive)) foundTraversal = true;
        }
        assert(foundTraversal);
        std::cout << "  -> PASSED: Path traversal attempts blocked." << std::endl;
    }

    // Test 8: Insufficient Storage / Failed Installation Does Not Appear Active
    {
        std::cout << "[Test 8] Testing insufficient storage aborts installation without partial state..." << std::endl;
        const QString validManifest = createFixturePackage("pkg_storage_test", "storage_reg", "Storage Test", 1, 10.0, 12.0, 20.0, 22.0, "state.map", "graph.json");
        manager.setSimulateStorageFull(true);

        QString err;
        assert(!manager.installPackage(validManifest, err));
        assert(err.contains("Insufficient storage"));
        assert(!manager.getPackage("storage_reg").has_value());
        manager.setSimulateStorageFull(false);
        std::cout << "  -> PASSED: Storage failure handled safely without corrupted state." << std::endl;
    }

    // Test 9: Successful Installation & Idempotent Duplicate Installation
    {
        std::cout << "[Test 9] Testing clean installation and idempotent re-installation..." << std::endl;
        const QString validManifest = createFixturePackage("pkg_install_uk", "uttarakhand", "Uttarakhand", 1, 28.7, 31.5, 77.5, 81.1, "state.map", "graph.json");
        QString err;
        assert(manager.installPackage(validManifest, err));
        auto pkg = manager.getPackage("uttarakhand");
        assert(pkg.has_value());
        assert(pkg->status == PackageStatus::Installed);
        assert(pkg->hasRenderingData);
        assert(pkg->hasRoutingData);

        // Reinstalling same package should cleanly overwrite and succeed
        assert(manager.installPackage(validManifest, err));
        auto pkgs = manager.listInstalledPackages();
        int count = 0;
        for (const auto &p : pkgs) {
            if (p.regionId == "uttarakhand") count++;
        }
        assert(count == 1); // No duplicate entries in repository
        std::cout << "  -> PASSED: Installation is clean, verified, and idempotent." << std::endl;
    }

    // Test 10: Failed Installation Preserves Previous Valid Package
    {
        std::cout << "[Test 10] Testing failed update preserves previous valid installation..." << std::endl;
        // Verify uttarakhand is currently installed
        auto prev = manager.getPackage("uttarakhand");
        assert(prev.has_value());

        // Attempt to install a corrupted update for uttarakhand
        const QString corruptUpdate = createFixturePackage("pkg_uk_corrupt", "uttarakhand", "Uttarakhand Corrupt", 2, 28.7, 31.5, 77.5, 81.1, "state.map", "graph.json", true, true);
        QString err;
        assert(!manager.installPackage(corruptUpdate, err));

        // Previous valid package must still exist and be intact
        auto preserved = manager.getPackage("uttarakhand");
        assert(preserved.has_value());
        assert(preserved->packageVersion == 1);
        std::cout << "  -> PASSED: Failed update preserves previous valid package." << std::endl;
    }

    // Test 11: Region Selection & Coverage Boundaries
    {
        std::cout << "[Test 11] Testing region selection and geographic coverage bounds..." << std::endl;
        assert(manager.activateRegion("uttarakhand"));
        assert(manager.hasActivePackage());
        assert(manager.activeRegionId() == "uttarakhand");
        assert(manager.activeRegionDisplayName() == "Uttarakhand");

        // Coordinates within Uttarakhand bounds (e.g. Dehradun: 30.3165, 78.0322)
        assert(manager.isCoordinateCovered(30.3165, 78.0322));
        assert(manager.isCoordinateInsideRegion("uttarakhand", 30.3165, 78.0322));

        // Coordinates outside Uttarakhand bounds (e.g. London: 51.5074, -0.1278)
        assert(!manager.isCoordinateCovered(51.5074, -0.1278));
        assert(!manager.isCoordinateInsideRegion("uttarakhand", 51.5074, -0.1278));
        std::cout << "  -> PASSED: Coverage boundaries verified accurately." << std::endl;
    }

    // Test 12: Restart and Metadata Recovery
    {
        std::cout << "[Test 12] Testing persistent metadata recovery after restart..." << std::endl;
        // Instantiate fresh manager on the same repository
        MapPackageManager restartedManager(repo, installDir);
        auto active = restartedManager.getActivePackage();
        assert(active.has_value());
        assert(active->regionId == "uttarakhand");
        assert(active->hasRenderingData);
        assert(active->hasRoutingData);
        assert(restartedManager.isCoordinateCovered(30.3165, 78.0322));
        std::cout << "  -> PASSED: Package metadata fully recovered on restart." << std::endl;
    }

    // Clean up temporary test files
    QDir(testTempDir).removeRecursively();

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 12 PHASE 5 MAP-PACKAGE TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
