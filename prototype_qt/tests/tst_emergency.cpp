#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QFile>
#include <QDir>

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

        auto updateRes = repo->updateLocation("user_loc", eid, validLoc, EmergencyLocationStatus::Available);
        assert(updateRes.isSuccess);
        assert(updateRes.data->location.has_value());
        assert(updateRes.data->location->latitude == 30.3165);
        assert(updateRes.data->location->longitude == 78.0322);
        assert(updateRes.data->locationStatus == EmergencyLocationStatus::Available);

        // Empty emergency ID update must be rejected
        auto badRes = repo->updateLocation("user_loc", "", validLoc, EmergencyLocationStatus::Available);
        assert(!badRes.isSuccess);
        assert(badRes.errorCode == EmergencyErrorCode::InvalidInput);

        // Empty user ID update must be rejected
        auto badUserRes = repo->updateLocation("", eid, validLoc, EmergencyLocationStatus::Available);
        assert(!badUserRes.isSuccess);
        assert(badUserRes.errorCode == EmergencyErrorCode::InvalidInput);
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

        auto noteRes = repo->updateNote("user_note", eid, longNote);
        assert(noteRes.isSuccess);
        assert(noteRes.data->note.length() == 120);
        assert(noteRes.data->note == longNote.left(120));

        // Empty user ID update must be rejected
        auto badNoteUser = repo->updateNote("", eid, "Distress Note");
        assert(!badNoteUser.isSuccess);
        assert(badNoteUser.errorCode == EmergencyErrorCode::InvalidInput);
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

    // Test 17: Emergency Ownership Validation (Cross-user mutation rejection)
    {
        std::cout << "[Test 17] Testing cross-user emergency ownership validation..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        // User A creates emergency
        auto resA = repo->activateEmergency("user_victim", "Victim distress signal");
        assert(resA.isSuccess);
        const QString victimEid = resA.data->emergencyId;

        // User B attempts to update User A's location
        EmergencyLocation fakeLoc;
        fakeLoc.latitude = 40.7128;
        fakeLoc.longitude = -74.0060;
        fakeLoc.accuracyMeters = 10.0;
        fakeLoc.timestampUtcMs = 1710000005000;
        auto hackLoc = repo->updateLocation("user_attacker", victimEid, fakeLoc, EmergencyLocationStatus::Available);
        assert(!hackLoc.isSuccess);
        assert(hackLoc.errorCode == EmergencyErrorCode::NotFound);
        // Error message must not disclose victim ID or details
        assert(!hackLoc.errorMessage.contains("user_victim"));

        // User B attempts to update User A's note
        auto hackNote = repo->updateNote("user_attacker", victimEid, "Attacker compromised note");
        assert(!hackNote.isSuccess);
        assert(hackNote.errorCode == EmergencyErrorCode::NotFound);
        assert(!hackNote.errorMessage.contains("user_victim"));

        // User B attempts to complete User A's emergency
        auto hackComp = repo->completeEmergency("user_attacker", victimEid);
        assert(!hackComp.isSuccess);
        assert(hackComp.errorCode == EmergencyErrorCode::NotFound);
        assert(!hackComp.errorMessage.contains("user_victim"));

        // Verify User A's emergency remains unmodified in SQLite
        auto checkVictim = repo->getActiveEmergency("user_victim");
        assert(checkVictim.isSuccess && checkVictim.data.has_value() && checkVictim.data.value().has_value());
        assert(checkVictim.data.value()->emergencyId == victimEid);
        assert(checkVictim.data.value()->status == EmergencyStatus::Active);
        assert(checkVictim.data.value()->note == "Victim distress signal");
        assert(!checkVictim.data.value()->location.has_value());
        std::cout << "  -> PASSED: Cross-user mutation strictly rejected without leaking record existence." << std::endl;
    }

    // Test 18: Inactive / Completed Emergency and Unknown ID Mutation Rejection
    {
        std::cout << "[Test 18] Testing completed emergency and unknown ID mutation rejection..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        auto repo = std::make_shared<SqliteEmergencyRepository>(db);

        // Activate and complete emergency
        auto act = repo->activateEmergency("user_state", "State test");
        assert(act.isSuccess);
        const QString eid = act.data->emergencyId;
        assert(repo->completeEmergency("user_state", eid).isSuccess);

        EmergencyLocation testLoc;
        testLoc.latitude = 34.0522;
        testLoc.longitude = -118.2437;
        testLoc.accuracyMeters = 8.0;
        testLoc.timestampUtcMs = 1710000010000;

        // Mutation on completed emergency must fail with InvalidState
        auto locOnComp = repo->updateLocation("user_state", eid, testLoc, EmergencyLocationStatus::Available);
        assert(!locOnComp.isSuccess);
        assert(locOnComp.errorCode == EmergencyErrorCode::InvalidState);

        auto noteOnComp = repo->updateNote("user_state", eid, "Should fail");
        assert(!noteOnComp.isSuccess);
        assert(noteOnComp.errorCode == EmergencyErrorCode::InvalidState);

        // Mutation on unknown emergency ID must fail with NotFound
        auto locUnknown = repo->updateLocation("user_state", "emer_nonexistent_999", testLoc, EmergencyLocationStatus::Available);
        assert(!locUnknown.isSuccess);
        assert(locUnknown.errorCode == EmergencyErrorCode::NotFound);

        auto noteUnknown = repo->updateNote("user_state", "emer_nonexistent_999", "Unknown note");
        assert(!noteUnknown.isSuccess);
        assert(noteUnknown.errorCode == EmergencyErrorCode::NotFound);
        std::cout << "  -> PASSED: Completed emergencies and unknown IDs safely reject mutations." << std::endl;
    }

    // Test 19: Citizen-Only C++ API Hardening (Admin Session Rejection)
    {
        std::cout << "[Test 19] Testing C++ API hardening against Admin session emergency operations..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        assert(db->initialize(":memory:"));
        AppState state(db);

        // Log in as Administrator
        assert(state.loginDevelopmentAdmin("Command Admin", "admin.commander@georescux.org"));
        assert(state.session()->isAdmin());
        assert(state.session()->currentRole() == UserRole::AuthorizedAdmin);

        // Admin cannot activate SOS
        assert(!state.activateSos("Admin attempt to trigger SOS"));
        assert(!state.isEmergencyActive());
        assert(state.emergencyService()->activeEmergencyId().isEmpty());

        // Admin cannot resolve SOS
        assert(!state.resolveSos());

        // Admin cannot update SOS note
        assert(!state.updateActiveNote("Admin note mutation"));

        // Verify SQLite database has 0 emergency records created for this admin
        QSqlDatabase rawDb = QSqlDatabase::database(db->connectionName());
        QSqlQuery qCount(rawDb);
        qCount.prepare("SELECT COUNT(*) FROM emergency_records WHERE user_id = :uid;");
        qCount.bindValue(":uid", state.session()->currentUserId());
        assert(qCount.exec());
        assert(qCount.next());
        assert(qCount.value(0).toInt() == 0);

        // Switch to Citizen User session
        assert(state.loginDevelopmentUser("Citizen Rescuer", "citizen.rescuer@georescux.org"));
        assert(!state.session()->isAdmin());
        assert(state.session()->currentRole() == UserRole::CitizenUser);

        // Citizen activation succeeds
        assert(state.activateSos("Valid Citizen SOS"));
        assert(state.isEmergencyActive());
        assert(!state.emergencyService()->activeEmergencyId().isEmpty());

        // Citizen note update succeeds
        assert(state.updateActiveNote("Citizen SOS details updated"));
        assert(state.emergencyService()->activeNote() == "Citizen SOS details updated");

        // Citizen resolution succeeds
        assert(state.resolveSos());
        assert(!state.isEmergencyActive());
        std::cout << "  -> PASSED: Admin sessions strictly rejected by C++ API; Citizen operations succeed." << std::endl;
    }

    // Test 20: Migration Rollback Failure Injection and Recovery
    {
        std::cout << "[Test 20] Testing Schema V1 -> V2 migration failure rollback and recovery..." << std::endl;
        const QString testDbPath = QDir::temp().filePath(QStringLiteral("georescux_test_migration_rollback.db"));
        if (QFile::exists(testDbPath)) {
            QFile::remove(testDbPath);
        }

        // Phase A: Trigger simulated failure during V1 -> V2 migration
        {
            LocalDatabase failingDb;
            failingDb.setSimulateMigrationFailure(true);
            bool initSuccess = failingDb.initialize(testDbPath);
            assert(!initSuccess); // Must report failure
            assert(!failingDb.isOpen());
            assert(!failingDb.lastError().isEmpty());
            assert(failingDb.lastError().contains("Simulated Schema V2 migration failure"));
        }

        // Phase B: Verify raw database state after rollback
        // Database file must exist (not deleted or wiped)
        assert(QFile::exists(testDbPath));

        {
            const QString inspectConn = QStringLiteral("migration_inspect_conn");
            {
                QSqlDatabase inspectDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), inspectConn);
                inspectDb.setDatabaseName(testDbPath);
                assert(inspectDb.open());

                // 1. Existing V1 data and schema_migrations table must remain intact
                QSqlQuery verQ(inspectDb);
                assert(verQ.exec("SELECT MAX(version) FROM schema_migrations;"));
                assert(verQ.next());
                // Version must NOT advance to 2; it must remain at 1
                assert(verQ.value(0).toInt() == 1);

                // 2. V1 tables must exist
                QSqlQuery v1TableQ(inspectDb);
                assert(v1TableQ.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='app_preferences';"));
                assert(v1TableQ.next());

                // 3. V2 transactional table creation must have been rolled back
                QSqlQuery v2TableQ(inspectDb);
                assert(v2TableQ.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='emergency_records';"));
                assert(!v2TableQ.next()); // Table must NOT exist!

                // 4. V2 index must NOT exist
                QSqlQuery v2IndexQ(inspectDb);
                assert(v2IndexQ.exec("SELECT name FROM sqlite_master WHERE type='index' AND name='idx_one_active_emergency_per_user';"));
                assert(!v2IndexQ.next());

                inspectDb.close();
            }
            QSqlDatabase::removeDatabase(inspectConn);
        }

        // Phase C: Recovery - retry migration without failure flag
        {
            LocalDatabase recoveryDb;
            recoveryDb.setSimulateMigrationFailure(false);
            bool recoverSuccess = recoveryDb.initialize(testDbPath);
            assert(recoverSuccess);
            assert(recoveryDb.isOpen());
            assert(recoveryDb.schemaVersion() == 2);

            // Verify emergency_records table and index now exist
            QSqlDatabase rawDb = QSqlDatabase::database(recoveryDb.connectionName());
            QSqlQuery checkV2Table(rawDb);
            assert(checkV2Table.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='emergency_records';"));
            assert(checkV2Table.next());

            QSqlQuery checkV2Index(rawDb);
            assert(checkV2Index.exec("SELECT name FROM sqlite_master WHERE type='index' AND name='idx_one_active_emergency_per_user';"));
            assert(checkV2Index.next());
        }

        // Clean up test file
        QFile::remove(testDbPath);
        std::cout << "  -> PASSED: Migration failure cleanly rolled back V2, preserved V1, and recovered successfully." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 20 PHASE 4 EMERGENCY & HARDENING TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
