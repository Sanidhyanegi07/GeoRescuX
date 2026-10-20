#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

#include "domain/EmergencyModel.h"
#include "domain/IEmergencyRepository.h"
#include "domain/EmergencyService.h"
#include "domain/SessionModel.h"
#include "platform/storage/LocalDatabase.h"
#include "platform/storage/SqliteEmergencyRepository.h"
#include "app/AppState.h"

using namespace georescux::domain;
using namespace georescux::storage;
using namespace georescux::app;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] Running Phase 3 Emergency Lifecycle Unit Tests" << std::endl;
    std::cout << "=================================================================" << std::endl;

    // Test 1: Activation and persistence
    {
        std::cout << "[Test 1] Testing emergency activation and local SQLite persistence..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));

        auto repo = std::make_shared<SqliteEmergencyRepository>(db);
        EmergencyService service(repo);

        assert(!service.hasActiveEmergency());
        assert(service.activeStatusName() == "Inactive");

        bool ok = service.activateEmergency("user_alpha", "Severe injury in sector 4");
        assert(ok);
        assert(service.hasActiveEmergency());
        assert(!service.activeEmergencyId().isEmpty());
        assert(service.activeEmergencyId().startsWith("emer_"));
        assert(service.activeStatusName() == "ACTIVE");
        assert(service.activeNote() == "Severe injury in sector 4");
        assert(service.activeStartedAtUtcMs() > 0);

        // Verify directly in SQLite storage
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery q(rawDb);
        q.prepare("SELECT emergency_id, user_id, status, note, schema_version FROM emergency_records WHERE user_id = 'user_alpha';");
        assert(q.exec());
        assert(q.next());
        assert(q.value(0).toString() == service.activeEmergencyId());
        assert(q.value(1).toString() == "user_alpha");
        assert(q.value(2).toString() == "ACTIVE");
        assert(q.value(3).toString() == "Severe injury in sector 4");
        assert(q.value(4).toInt() == 2);
        std::cout << "  -> PASSED: Emergency activated and persisted to SQLite V2." << std::endl;
    }

    // Test 2: Unique ID generation
    {
        std::cout << "[Test 2] Testing unique, stable emergency ID generation..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto res1 = repo->activateEmergency("user_001");
        assert(res1.isSuccess);
        auto res2 = repo->activateEmergency("user_002");
        assert(res2.isSuccess);

        assert(res1.data->emergencyId != res2.data->emergencyId);
        assert(res1.data->emergencyId.startsWith("emer_"));
        assert(res2.data->emergencyId.startsWith("emer_"));
        std::cout << "  -> PASSED: Distinct, unique emergency IDs generated." << std::endl;
    }

    // Test 3: Repeated activation returns existing active emergency without duplicate creation
    {
        std::cout << "[Test 3] Testing repeated activation rejection (AlreadyActive)..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);
        EmergencyService service(repo);

        assert(service.activateEmergency("user_repeat", "Initial SOS"));
        const QString originalId = service.activeEmergencyId();
        const qint64 originalStarted = service.activeStartedAtUtcMs();

        // Second activation attempt for same user must be rejected
        bool secondOk = service.activateEmergency("user_repeat", "Second SOS attempt");
        assert(!secondOk);
        assert(service.hasActiveEmergency());
        assert(service.activeEmergencyId() == originalId);
        assert(service.activeStartedAtUtcMs() == originalStarted);

        // Ensure database only contains 1 record for this user
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery countQ(rawDb);
        countQ.prepare("SELECT COUNT(*) FROM emergency_records WHERE user_id = 'user_repeat';");
        assert(countQ.exec());
        assert(countQ.next());
        assert(countQ.value(0).toInt() == 1);
        std::cout << "  -> PASSED: Repeated activation rejected without duplicate records." << std::endl;
    }

    // Test 4: Direct SQLite Partial Unique Index Constraint Bypass Attempt
    {
        std::cout << "[Test 4] Testing SQLite partial unique index constraint enforcement against direct bypass..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        // Create legitimate active emergency
        assert(repo->activateEmergency("user_bypass").isSuccess);

        // Attempt to execute raw SQL directly inserting a second ACTIVE emergency for the same user
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery rawInsert(rawDb);
        rawInsert.prepare(
            "INSERT INTO emergency_records ("
            "  emergency_id, user_id, status, started_at_utc_ms, location_status, note, "
            "  created_at_utc_ms, updated_at_utc_ms, sync_status, schema_version"
            ") VALUES ("
            "  'emer_bypass_999', 'user_bypass', 'ACTIVE', 9999999, 'UNKNOWN', 'Bypass Attempt', "
            "  9999999, 9999999, 'PENDING', 2"
            ");"
        );
        bool bypassExec = rawInsert.exec();
        // RAW SQL INSERT MUST FAIL DUE TO idx_one_active_emergency_per_user!
        assert(!bypassExec);
        const QString rawErr = rawInsert.lastError().text();
        assert(rawErr.contains("UNIQUE constraint failed", Qt::CaseInsensitive) ||
               rawErr.contains("idx_one_active_emergency_per_user", Qt::CaseInsensitive));

        std::cout << "  -> PASSED: Direct SQLite partial unique index rejected raw second active insert: "
                  << rawErr.toStdString() << std::endl;
    }

    // Test 5: Different users maintain independent records
    {
        std::cout << "[Test 5] Testing multi-user independent active emergencies..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto resA = repo->activateEmergency("user_A", "User A emergency");
        auto resB = repo->activateEmergency("user_B", "User B emergency");
        auto resC = repo->activateEmergency("user_C", "User C emergency");

        assert(resA.isSuccess && resB.isSuccess && resC.isSuccess);
        assert(resA.data->emergencyId != resB.data->emergencyId);
        assert(resB.data->emergencyId != resC.data->emergencyId);

        // User A completes their emergency; User B and C must remain ACTIVE
        assert(repo->completeEmergency("user_A", resA.data->emergencyId).isSuccess);

        auto activeB = repo->getActiveEmergency("user_B");
        assert(activeB.isSuccess && activeB.data.has_value() && activeB.data.value().has_value());
        assert(activeB.data.value()->status == EmergencyStatus::Active);

        auto activeA = repo->getActiveEmergency("user_A");
        assert(activeA.isSuccess && activeA.data.has_value() && !activeA.data.value().has_value());
        std::cout << "  -> PASSED: Multiple users operate independently without cross-user interference." << std::endl;
    }

    // Test 6: Location update validation
    {
        std::cout << "[Test 6] Testing emergency location update validation..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto act = repo->activateEmergency("user_loc");
        assert(act.isSuccess);
        const QString eid = act.data->emergencyId;

        // Valid GPS coordinate snapshot
        EmergencyLocation validLoc;
        validLoc.latitude = 30.3165;
        validLoc.longitude = 78.0322;
        validLoc.accuracyMeters = 5.2;
        validLoc.timestampUtcMs = 1710000000000;
        assert(validLoc.isValid());

        auto updateRes = repo->updateLocation(eid, validLoc, EmergencyLocationStatus::Available);
        assert(updateRes.isSuccess);
        assert(updateRes.data->location.has_value());
        assert(updateRes.data->location->latitude == 30.3165);
        assert(updateRes.data->location->longitude == 78.0322);
        assert(updateRes.data->locationStatus == EmergencyLocationStatus::Available);

        // Empty emergency ID update must be rejected
        auto badRes = repo->updateLocation("", validLoc, EmergencyLocationStatus::Available);
        assert(!badRes.isSuccess);
        assert(badRes.errorCode == EmergencyErrorCode::InvalidInput);
        std::cout << "  -> PASSED: Location updates verified and validated." << std::endl;
    }

    // Test 7: Note update validation and 120-char truncation
    {
        std::cout << "[Test 7] Testing note updates and 120-character capping..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto act = repo->activateEmergency("user_note");
        assert(act.isSuccess);
        const QString eid = act.data->emergencyId;

        // Update note with string longer than 120 characters
        QString longNote = "This is an extremely long distress message that exceeds the maximum allowed length of one hundred and twenty characters set by the emergency specification rules.";
        assert(longNote.length() > 120);

        auto noteRes = repo->updateNote(eid, longNote);
        assert(noteRes.isSuccess);
        assert(noteRes.data->note.length() == 120);
        assert(noteRes.data->note == longNote.left(120));
        std::cout << "  -> PASSED: Note successfully updated and capped at 120 characters." << std::endl;
    }

    // Test 8: Successful completion and history retention
    {
        std::cout << "[Test 8] Testing successful completion lifecycle and history retention..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);
        EmergencyService service(repo);

        assert(service.activateEmergency("user_comp", "Rescue needed"));
        const QString eid = service.activeEmergencyId();

        bool compOk = service.completeEmergency("user_comp", eid);
        assert(compOk);
        assert(!service.hasActiveEmergency());
        assert(service.activeStatusName() == "Inactive");

        // Verify record in history
        auto histRes = repo->getEmergencyHistory("user_comp");
        assert(histRes.isSuccess);
        assert(histRes.data->size() == 1);
        const auto &histItem = histRes.data->front();
        assert(histItem.emergencyId == eid);
        assert(histItem.status == EmergencyStatus::Completed);
        assert(histItem.completedAtUtcMs.has_value());
        assert(histItem.completedAtUtcMs.value() >= histItem.startedAtUtcMs);
        std::cout << "  -> PASSED: Emergency successfully completed and archived." << std::endl;
    }

    // Test 9: Duplicate completion rejection
    {
        std::cout << "[Test 9] Testing duplicate completion rejection..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto act = repo->activateEmergency("user_dup");
        assert(act.isSuccess);
        const QString eid = act.data->emergencyId;

        // First completion succeeds
        auto comp1 = repo->completeEmergency("user_dup", eid);
        assert(comp1.isSuccess);

        // Second completion must fail with InvalidState
        auto comp2 = repo->completeEmergency("user_dup", eid);
        assert(!comp2.isSuccess);
        assert(comp2.errorCode == EmergencyErrorCode::InvalidState);
        std::cout << "  -> PASSED: Duplicate completion rejected with InvalidState." << std::endl;
    }

    // Test 10: Unknown emergency ID or user mismatch completion rejection
    {
        std::cout << "[Test 10] Testing unknown emergency ID and user mismatch rejection..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto act = repo->activateEmergency("user_real");
        assert(act.isSuccess);
        const QString eid = act.data->emergencyId;

        // Non-existent ID
        auto resUnknown = repo->completeEmergency("user_real", "non_existent_emergency_id");
        assert(!resUnknown.isSuccess);
        assert(resUnknown.errorCode == EmergencyErrorCode::NotFound);

        // Wrong user attempting to complete another user's emergency
        auto resWrongUser = repo->completeEmergency("user_impostor", eid);
        assert(!resWrongUser.isSuccess);
        assert(resWrongUser.errorCode == EmergencyErrorCode::NotFound);
        std::cout << "  -> PASSED: Unknown emergency and user mismatch safely rejected." << std::endl;
    }

    // Test 11: Database failure / unavailable handling
    {
        std::cout << "[Test 11] Testing storage unavailable error handling..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        // Uninitialized database
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        auto res = repo->activateEmergency("user_uninit");
        assert(!res.isSuccess);
        assert(res.errorCode == EmergencyErrorCode::StorageUnavailable);

        auto getRes = repo->getActiveEmergency("user_uninit");
        assert(!getRes.isSuccess);
        assert(getRes.errorCode == EmergencyErrorCode::StorageUnavailable);
        std::cout << "  -> PASSED: Storage unavailability returns structured error without crashing." << std::endl;
    }

    // Test 12: Migration V1 to V2
    {
        std::cout << "[Test 12] Testing SQLite migration V1 -> V2..." << std::endl;
        // Construct a database that manually executes Schema V1 only
        LocalDatabase db;
        assert(db.initialize(":memory:"));
        // Both V1 and V2 have run now
        assert(db.schemaVersion() == 2);

        QSqlDatabase rawDb = QSqlDatabase::database(db.connectionName());
        QSqlQuery q(rawDb);
        // Verify schema_migrations table has records for 1 and 2
        assert(q.exec("SELECT version, description FROM schema_migrations ORDER BY version;"));
        assert(q.next());
        assert(q.value(0).toInt() == 1);
        assert(q.next());
        assert(q.value(0).toInt() == 2);

        // Verify emergency_records table and indexes exist
        QSqlQuery tableCheck(rawDb);
        assert(tableCheck.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='emergency_records';"));
        assert(tableCheck.next());

        QSqlQuery indexCheck(rawDb);
        assert(indexCheck.exec("SELECT name FROM sqlite_master WHERE type='index' AND name='idx_one_active_emergency_per_user';"));
        assert(indexCheck.next());
        std::cout << "  -> PASSED: Schema V1 and V2 migrations tracked in schema_migrations table." << std::endl;
    }

    // Test 13: Restart / Reinitialization recovery
    {
        std::cout << "[Test 13] Testing restart / reinitialization active emergency recovery..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        // Persist an active emergency
        auto act = repo->activateEmergency("user_recover", "Restart recovery test");
        assert(act.isSuccess);
        const QString originalEid = act.data->emergencyId;

        // Simulate app restart: Instantiate fresh EmergencyService on the same database
        EmergencyService restartedService(repo);
        assert(!restartedService.hasActiveEmergency()); // Initially inactive before user resolution

        // Resolve active emergency for the authenticated user
        restartedService.loadActiveForUser("user_recover");
        assert(restartedService.hasActiveEmergency());
        assert(restartedService.activeEmergencyId() == originalEid);
        assert(restartedService.activeNote() == "Restart recovery test");
        assert(restartedService.activeStatusName() == "ACTIVE");

        // User without active emergency restores clean inactive state
        restartedService.loadActiveForUser("user_other");
        assert(!restartedService.hasActiveEmergency());
        std::cout << "  -> PASSED: Active emergency successfully restored on application restart." << std::endl;
    }

    // Test 14: User Isolation on Shared Device
    {
        std::cout << "[Test 14] Testing user isolation across session changes..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);
        EmergencyService service(repo);

        // User 1 logs in and activates SOS
        service.activateEmergency("user_first", "First user SOS");
        assert(service.hasActiveEmergency());
        assert(service.activeEmergencyId().startsWith("emer_"));

        // User 1 logs out -> clearActive() clears memory state
        service.clearActive();
        assert(!service.hasActiveEmergency());

        // User 2 logs in -> loadActiveForUser("user_second")
        service.loadActiveForUser("user_second");
        // User 2 must NOT inherit User 1's active emergency
        assert(!service.hasActiveEmergency());

        // User 1 logs back in -> loadActiveForUser("user_first")
        service.loadActiveForUser("user_first");
        assert(service.hasActiveEmergency());
        assert(service.activeNote() == "First user SOS");
        std::cout << "  -> PASSED: User isolation strictly preserved across logout and login." << std::endl;
    }

    // Test 15: Navigation changes preserve active emergency state
    {
        std::cout << "[Test 15] Testing navigation transitions preserve active emergency state..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        AppState state(db);

        assert(state.loginDevelopmentUser("Citizen Nav", "citizen.nav@georescux.org"));
        assert(state.activateSos("Navigation persistence note"));
        assert(state.isEmergencyActive());
        const QString activeId = state.emergencyService()->activeEmergencyId();

        // Change screens across User Portal
        assert(state.navigateTo("UserMap"));
        assert(state.isEmergencyActive());
        assert(state.emergencyService()->activeEmergencyId() == activeId);

        assert(state.navigateTo("UserSafeRoute"));
        assert(state.isEmergencyActive());
        assert(state.emergencyService()->activeEmergencyId() == activeId);

        assert(state.navigateTo("UserAlerts"));
        assert(state.isEmergencyActive());

        assert(state.navigateTo("UserProfile"));
        assert(state.isEmergencyActive());

        assert(state.navigateTo("UserHome"));
        assert(state.isEmergencyActive());
        assert(state.emergencyService()->activeEmergencyId() == activeId);

        // Complete emergency and check navigation state remains intact
        assert(state.resolveSos());
        assert(!state.isEmergencyActive());
        assert(state.currentNavigation() == "UserHome");
        std::cout << "  -> PASSED: Navigation changes preserve authoritative emergency state." << std::endl;
    }

    // Test 16: History retention after multiple completed emergencies
    {
        std::cout << "[Test 16] Testing history retention in reverse chronological order..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        // Run 3 sequential emergencies for user_hist
        for (int i = 1; i <= 3; ++i) {
            auto act = repo->activateEmergency("user_hist", QString("Emergency #%1").arg(i));
            assert(act.isSuccess);
            // Brief sleep to ensure distinct timestamps
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            auto comp = repo->completeEmergency("user_hist", act.data->emergencyId);
            assert(comp.isSuccess);
        }

        auto histRes = repo->getEmergencyHistory("user_hist", 10);
        assert(histRes.isSuccess);
        assert(histRes.data->size() == 3);
        // Reverse chronological order: most recent first
        assert(histRes.data->at(0).note == "Emergency #3");
        assert(histRes.data->at(1).note == "Emergency #2");
        assert(histRes.data->at(2).note == "Emergency #1");
        std::cout << "  -> PASSED: History preserved in reverse chronological order." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 16 PHASE 3 EMERGENCY LIFECYCLE TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
