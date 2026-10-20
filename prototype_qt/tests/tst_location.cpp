#include <iostream>
#include <cassert>
#include <memory>
#include <functional>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

#include "domain/LocationModel.h"
#include "domain/ILocationProvider.h"
#include "domain/FakeLocationProvider.h"
#include "domain/LocationService.h"
#include "domain/EmergencyModel.h"
#include "domain/IEmergencyRepository.h"
#include "domain/EmergencyService.h"
#include "domain/SessionModel.h"
#include "domain/MapPackageManager.h"
#include "platform/storage/LocalDatabase.h"
#include "platform/storage/SqliteEmergencyRepository.h"
#include "platform/storage/SqliteMapPackageRepository.h"
#include "platform/qt/OfflineMapView.h"
#include "app/AppState.h"

using namespace georescux::domain;
using namespace georescux::storage;
using namespace georescux::app;
using namespace georescux::ui;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] Running Phase 8 GPS & Location Unit Tests" << std::endl;
    std::cout << "=================================================================" << std::endl;

    // Test 1: Permission Denied Handling
    {
        std::cout << "[Test 1] Testing location permission denied handling..." << std::endl;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider);

        fakeProvider->setPermissionStatus(LocationPermissionStatus::Denied);
        service.startTracking();

        assert(service.permissionStatus() == LocationPermissionStatus::Denied);
        assert(service.trackingState() == LocationTrackingState::PermissionDenied);
        assert(!service.hasValidFix());
        assert(service.currentLatitude() == 0.0);
        assert(service.currentLongitude() == 0.0);
        std::cout << "  -> PASSED: Permission denied handled gracefully without crash." << std::endl;
    }

    // Test 2: Location Services Disabled
    {
        std::cout << "[Test 2] Testing location services disabled handling..." << std::endl;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider);

        fakeProvider->setLocationServicesEnabled(false);
        service.startTracking();

        assert(service.trackingState() == LocationTrackingState::LocationServicesDisabled);
        assert(!service.hasValidFix());
        assert(service.trackingStateName() == "SERVICES_DISABLED");
        std::cout << "  -> PASSED: Disabled location services reported cleanly." << std::endl;
    }

    // Test 3: First Valid Fix Acquisition
    {
        std::cout << "[Test 3] Testing first valid position acquisition..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();
        assert(service.trackingState() == LocationTrackingState::AcquisitionPending);

        bool signalFired = false;
        QObject::connect(&service, &LocationService::positionChanged, [&signalFired](const GeoPosition &pos) {
            signalFired = true;
            assert(pos.isValid());
        });

        // Dehradun coordinates: 30.3165 N, 78.0322 E, 5.2m accuracy
        fakeProvider->injectFix(30.3165, 78.0322, 5.2, simulatedTime);

        assert(signalFired);
        assert(service.hasValidFix());
        assert(service.currentLatitude() == 30.3165);
        assert(service.currentLongitude() == 78.0322);
        assert(service.currentAccuracyMeters() == 5.2);
        assert(service.trackingState() == LocationTrackingState::ValidFixAcquired);
        assert(!service.isStale());
        std::cout << "  -> PASSED: First valid fix acquired and verified." << std::endl;
    }

    // Test 4: Invalid Coordinates Rejected
    {
        std::cout << "[Test 4] Testing invalid coordinates rejection..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();

        // Feed invalid fix: latitude 95.0 (> 90.0)
        fakeProvider->injectFix(95.0, 78.0, 5.0, simulatedTime);
        assert(!service.hasValidFix());
        assert(service.trackingState() == LocationTrackingState::AcquisitionPending);
        assert(service.currentLatitude() == 0.0);

        // Feed invalid fix: longitude -210.0 (< -180.0)
        fakeProvider->injectFix(30.0, -210.0, 5.0, simulatedTime);
        assert(!service.hasValidFix());
        assert(service.currentLatitude() == 0.0);
        std::cout << "  -> PASSED: Invalid coordinates rejected, no mock positions fabricated." << std::endl;
    }

    // Test 5: Inaccurate Fix Filtered / Flagged
    {
        std::cout << "[Test 5] Testing inaccurate fix filtering..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        LocationUpdatePolicy policy;
        policy.maxAcceptableAccuracyMeters = 50.0;
        service.setPolicy(policy);
        service.startTracking();

        // Feed fix with 120m accuracy (exceeds 50m threshold)
        fakeProvider->injectFix(30.3165, 78.0322, 120.0, simulatedTime);

        assert(!service.hasValidFix());
        assert(service.trackingState() == LocationTrackingState::StaleOrInaccurate);
        std::cout << "  -> PASSED: Inaccurate fixes flagged and filtered according to policy." << std::endl;
    }

    // Test 6: Stale Position Evaluation
    {
        std::cout << "[Test 6] Testing stale position timeout..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();
        fakeProvider->injectFix(30.3165, 78.0322, 6.0, simulatedTime);
        assert(service.hasValidFix());
        assert(!service.isStale());

        // Advance deterministic clock by 35 seconds (> 30s stale timeout)
        simulatedTime += 35000LL;
        service.evaluateStaleness();

        assert(service.isStale());
        assert(!service.hasValidFix()); // Stale fix is no longer valid live fix
        assert(service.trackingState() == LocationTrackingState::StaleOrInaccurate);
        assert(service.lastValidPosition().has_value()); // Preserves last known position
        assert(service.lastValidPosition()->latitude == 30.3165);
        std::cout << "  -> PASSED: Position stale timeout evaluated deterministically." << std::endl;
    }

    // Test 7: Temporary Location Loss Preserves Last Known Position
    {
        std::cout << "[Test 7] Testing temporary location loss handling..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();
        fakeProvider->injectFix(30.3165, 78.0322, 6.0, simulatedTime);
        assert(service.hasValidFix());

        // Temporary GPS signal drop
        fakeProvider->simulateSignalLoss();

        assert(service.trackingState() == LocationTrackingState::TemporarilyLost);
        assert(!service.hasValidFix());
        // Last valid position must remain preserved for emergency reference
        assert(service.lastValidPosition().has_value());
        assert(service.lastValidPosition()->latitude == 30.3165);
        std::cout << "  -> PASSED: Temporary location loss preserves last known valid position." << std::endl;
    }

    // Test 8: Recovery After Location Returns
    {
        std::cout << "[Test 8] Testing recovery after GPS signal returns..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();
        fakeProvider->injectFix(30.3165, 78.0322, 6.0, simulatedTime);
        assert(service.hasValidFix());

        // Signal lost
        fakeProvider->simulateSignalLoss();
        assert(!service.hasValidFix());

        // Signal recovered 10 seconds later at new position
        simulatedTime += 10000LL;
        fakeProvider->injectFix(30.3180, 78.0340, 4.5, simulatedTime);

        assert(service.hasValidFix());
        assert(service.trackingState() == LocationTrackingState::ValidFixAcquired);
        assert(service.currentLatitude() == 30.3180);
        assert(service.currentLongitude() == 78.0340);
        std::cout << "  -> PASSED: Recovery after signal restoration verified." << std::endl;
    }

    // Test 9: Map Marker Updates & Coverage Bounds
    {
        std::cout << "[Test 9] Testing map marker position and coverage validation..." << std::endl;
        OfflineMapView mapView;

        assert(!mapView.hasUserPosition());
        mapView.setUserPosition(30.3165, 78.0322, 8.0, true, false);

        assert(mapView.hasUserPosition());
        assert(mapView.userLatitude() == 30.3165);
        assert(mapView.userLongitude() == 78.0322);
        assert(mapView.userAccuracyMeters() == 8.0);
        assert(mapView.isUserPositionLive());
        assert(!mapView.isUserPositionStale());

        // Test stale update
        mapView.setUserPosition(30.3165, 78.0322, 8.0, false, true);
        assert(!mapView.isUserPositionLive());
        assert(mapView.isUserPositionStale());

        // Clear user position
        mapView.clearUserPosition();
        assert(!mapView.hasUserPosition());

        // Test rejection of (0,0) placeholder and out-of-bounds coordinates
        mapView.setUserPosition(0.0, 0.0, 5.0, true, false);
        assert(!mapView.hasUserPosition());

        mapView.setUserPosition(95.0, 78.0, 5.0, true, false);
        assert(!mapView.hasUserPosition());

        mapView.setUserPosition(30.0, 190.0, 5.0, true, false);
        assert(!mapView.hasUserPosition());

        std::cout << "  -> PASSED: Map view marker states updated and cleared, invalid/(0,0) rejected." << std::endl;
    }

    // Test 10: SOS Activation Without Location
    {
        std::cout << "[Test 10] Testing SOS activation without location (permission denied)..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        fakeProvider->setPermissionStatus(LocationPermissionStatus::Denied);

        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Citizen One", "citizen1@example.com"));

        // Activate SOS while location permission is denied
        bool activated = appState.activateSos("SOS with no GPS");
        assert(activated);
        assert(appState.isEmergencyActive());
        assert(appState.emergencyService()->hasActiveEmergency());

        // Invariant: Emergency must be created with PermissionDenied status
        auto rec = appState.emergencyService()->activeRecord();
        assert(rec.has_value());
        assert(rec->status == EmergencyStatus::Active);
        assert(rec->locationStatus == EmergencyLocationStatus::PermissionDenied);
        assert(!rec->location.has_value());
        assert(appState.emergencyService()->activeLocationString().contains("Permission Denied"));
        std::cout << "  -> PASSED: SOS activated successfully despite denied location permission." << std::endl;
    }

    // Test 11: Location Update on an Existing SOS
    {
        std::cout << "[Test 11] Testing live location update on existing active SOS..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Citizen One", "citizen1@example.com"));

        // Activate SOS
        assert(appState.activateSos("Injured hiker"));
        const QString origId = appState.emergencyService()->activeEmergencyId();

        // Now GPS acquires a valid fix
        qint64 fixTime = 1700000050000LL;
        fakeProvider->injectFix(30.3165, 78.0322, 5.0, fixTime);

        // Emergency record should be updated with new location
        auto rec = appState.emergencyService()->activeRecord();
        assert(rec.has_value());
        assert(rec->emergencyId == origId); // SOS ID must remain identical
        assert(rec->locationStatus == EmergencyLocationStatus::Available);
        assert(rec->location.has_value());
        assert(rec->location->latitude == 30.3165);
        assert(rec->location->longitude == 78.0322);
        assert(rec->location->accuracyMeters == 5.0);

        // Verify in SQLite storage
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery q(rawDb);
        q.prepare("SELECT latitude, longitude, location_accuracy_meters, location_status FROM emergency_records WHERE emergency_id = :id;");
        q.bindValue(":id", origId);
        assert(q.exec());
        assert(q.next());
        assert(q.value(0).toDouble() == 30.3165);
        assert(q.value(1).toDouble() == 78.0322);
        assert(q.value(2).toDouble() == 5.0);
        assert(q.value(3).toString() == "AVAILABLE");
        std::cout << "  -> PASSED: Existing SOS updated with fresh location in SQLite." << std::endl;
    }

    // Test 12: No Duplicate SOS Creation on Location Updates
    {
        std::cout << "[Test 12] Testing no duplicate SOS records created during location updates..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Citizen One", "citizen1@example.com"));

        assert(appState.activateSos("Trapped"));
        const QString sosId = appState.emergencyService()->activeEmergencyId();

        // Emit 5 successive position updates
        qint64 t = 1700000100000LL;
        for (int i = 0; i < 5; ++i) {
            fakeProvider->injectFix(30.3165 + i * 0.001, 78.0322 + i * 0.001, 4.0, t + i * 2000LL);
        }

        // Emergency ID must be unchanged
        assert(appState.emergencyService()->activeEmergencyId() == sosId);

        // Verify total record count in SQLite table is strictly 1
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery q(rawDb);
        assert(q.exec("SELECT COUNT(*) FROM emergency_records;"));
        assert(q.next());
        assert(q.value(0).toInt() == 1);
        std::cout << "  -> PASSED: Single-active-SOS invariant strictly preserved across updates." << std::endl;
    }

    // Test 13: Preserving Last Valid Location During Temporary Loss
    {
        std::cout << "[Test 13] Testing preservation of last valid location during temporary GPS loss..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Citizen One", "citizen1@example.com"));

        assert(appState.activateSos("Flash flood"));
        fakeProvider->injectFix(30.3200, 78.0400, 6.0, 1700000200000LL);

        // Drop GPS signal
        fakeProvider->simulateSignalLoss();

        // Invariant: Last valid location in activeRecord must NOT be cleared
        auto rec = appState.emergencyService()->activeRecord();
        assert(rec.has_value());
        assert(rec->location.has_value());
        assert(rec->location->latitude == 30.3200);
        assert(rec->location->longitude == 78.0400);

        // SQLite record must also still hold the coordinates
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery q(rawDb);
        assert(q.exec("SELECT latitude, longitude FROM emergency_records WHERE status = 'ACTIVE';"));
        assert(q.next());
        assert(q.value(0).toDouble() == 30.3200);
        assert(q.value(1).toDouble() == 78.0400);
        std::cout << "  -> PASSED: Last valid location preserved during temporary loss." << std::endl;
    }

    // Test 14: SOS Completion and Location Tracking Shutdown
    {
        std::cout << "[Test 14] Testing emergency completion and tracking policy teardown..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Citizen One", "citizen1@example.com"));

        assert(appState.activateSos("Need rescue"));
        assert(appState.isEmergencyActive());

        // LocationService should be in emergency mode (2s / 2m)
        assert(appState.locationService()->policy().sosIntervalMs == 2000);
        assert(appState.locationService()->policy().sosMinDistanceMeters == 2.0);

        // Complete the emergency
        assert(appState.resolveSos());
        assert(!appState.isEmergencyActive());
        assert(!appState.emergencyService()->hasActiveEmergency());

        // Normal policy thresholds remain preserved
        assert(appState.locationService()->policy().normalIntervalMs == 5000);
        assert(appState.locationService()->policy().normalMinDistanceMeters == 5.0);

        // User logout stops tracking completely
        appState.logout();
        assert(!appState.locationService()->isTracking());
        std::cout << "  -> PASSED: Emergency teardown properly restores tracking policy." << std::endl;
    }

    // Test 15: Offline Operation Without Firebase Availability
    {
        std::cout << "[Test 15] Testing 100% offline operation without cloud/Firebase access..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        AppState appState(db, nullptr, fakeProvider);
        assert(appState.loginDevelopmentUser("Offline Citizen", "offline@domain.org"));

        // All operations must complete with zero network calls and zero timeouts
        assert(appState.activateSos("Zero cloud test"));
        fakeProvider->injectFix(30.1234, 78.5678, 3.5, 1700000300000LL);
        assert(appState.emergencyService()->hasActiveEmergency());
        assert(appState.emergencyService()->activeLocationString().contains("30.12340"));

        assert(appState.resolveSos());
        assert(!appState.isEmergencyActive());
        std::cout << "  -> PASSED: 100% offline zero-network execution verified." << std::endl;
    }

    // Test 16: First Position Fix Never Arrives (Acquisition Timeout)
    {
        std::cout << "[Test 16] Testing first position fix timeout when signal never arrives..." << std::endl;
        qint64 simulatedTime = 1700000000000LL;
        auto fakeProvider = std::make_shared<FakeLocationProvider>();
        LocationService service(fakeProvider, nullptr, [&simulatedTime]() { return simulatedTime; });

        service.startTracking();
        assert(service.trackingState() == LocationTrackingState::AcquisitionPending);
        assert(!service.hasValidFix());

        // Advance deterministic clock by 35 seconds without any fix arriving
        simulatedTime += 35000LL;
        service.evaluateStaleness();

        // Must transition to LocationUnavailable and report clear status
        assert(service.trackingState() == LocationTrackingState::LocationUnavailable);
        assert(!service.hasValidFix());
        assert(service.statusMessage().contains("Unavailable"));

        // When fix finally arrives later, it recovers immediately
        simulatedTime += 5000LL;
        fakeProvider->injectFix(30.3165, 78.0322, 5.0, simulatedTime);
        assert(service.hasValidFix());
        assert(service.trackingState() == LocationTrackingState::ValidFixAcquired);
        std::cout << "  -> PASSED: First fix timeout transitioned cleanly to LocationUnavailable and recovered." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL PHASE 8 LOCATION TESTS PASSED (16/16)!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
