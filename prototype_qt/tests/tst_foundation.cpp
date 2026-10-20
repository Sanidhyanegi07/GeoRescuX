#include <iostream>
#include <cassert>
#include <memory>
#include <thread>
#include <QCoreApplication>
#include <QDateTime>

#include "domain/SessionModel.h"
#include "platform/storage/LocalDatabase.h"
#include "app/AppState.h"

using namespace georescux::domain;
using namespace georescux::storage;
using namespace georescux::app;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] Running Phase 2 Foundation & Navigation Tests" << std::endl;
    std::cout << "=================================================================" << std::endl;

    // Test 1: Initial SessionModel state
    {
        std::cout << "[Test 1] Verifying initial unauthenticated session state..." << std::endl;
        SessionModel session;
        assert(session.sessionState() == SessionState::Unauthenticated);
        assert(session.currentRole() == UserRole::None);
        assert(!session.isAuthenticated());
        assert(!session.isAdmin());
        assert(session.currentUserId().isEmpty());
        std::cout << "  -> PASSED: Initial session is Unauthenticated with UserRole::None." << std::endl;
    }

    // Test 2: Citizen User login transition & properties
    {
        std::cout << "[Test 2] Testing Citizen User login transition..." << std::endl;
        SessionModel session;
        bool ok = session.loginDevelopmentUser("Aarav Sharma", "aarav@example.com", "+91-9876543210");
        assert(ok);
        assert(session.sessionState() == SessionState::AuthenticatedUser);
        assert(session.currentRole() == UserRole::CitizenUser);
        assert(session.isAuthenticated());
        assert(!session.isAdmin());
        assert(session.currentUserName() == "Aarav Sharma");
        assert(session.currentUserEmail() == "aarav@example.com");
        assert(!session.currentUserId().isEmpty());
        std::cout << "  -> PASSED: Citizen User login transitions state to AuthenticatedUser." << std::endl;
    }

    // Test 3: Authorized Admin login transition & properties
    {
        std::cout << "[Test 3] Testing Authorized Admin login transition..." << std::endl;
        SessionModel session;
        bool ok = session.loginDevelopmentAdmin("Dr. Neha Verma", "neha.admin@georescux.org");
        assert(ok);
        assert(session.sessionState() == SessionState::AuthenticatedAdmin);
        assert(session.currentRole() == UserRole::AuthorizedAdmin);
        assert(session.isAuthenticated());
        assert(session.isAdmin());
        assert(session.currentUserName() == "Dr. Neha Verma");
        std::cout << "  -> PASSED: Admin login transitions state to AuthenticatedAdmin." << std::endl;
    }

    // Test 4: Invalid login transitions (empty credentials)
    {
        std::cout << "[Test 4] Testing rejection of invalid login arguments..." << std::endl;
        SessionModel session;
        bool okEmpty = session.loginDevelopmentUser("", "");
        assert(!okEmpty);
        assert(session.sessionState() == SessionState::SessionError);
        assert(!session.lastError().isEmpty());

        bool okAdminEmpty = session.loginDevelopmentAdmin("   ", "");
        assert(!okAdminEmpty);
        std::cout << "  -> PASSED: Invalid login rejected with appropriate SessionError." << std::endl;
    }

    // Test 5: Logout resets state cleanly
    {
        std::cout << "[Test 5] Testing logout cleanup..." << std::endl;
        SessionModel session;
        session.loginDevelopmentUser("Rahul", "rahul@test.com");
        assert(session.isAuthenticated());
        session.logout();
        assert(session.sessionState() == SessionState::Unauthenticated);
        assert(session.currentRole() == UserRole::None);
        assert(!session.isAuthenticated());
        assert(session.currentUserId().isEmpty());
        std::cout << "  -> PASSED: Logout cleanly restores Unauthenticated state." << std::endl;
    }

    // Test 6: LocalDatabase SQLite initialization & schema versioning
    {
        std::cout << "[Test 6] Testing LocalDatabase in-memory initialization & schema V2..." << std::endl;
        LocalDatabase db;
        bool ok = db.initialize(":memory:");
        assert(ok);
        assert(db.isOpen());
        assert(db.schemaVersion() == LocalDatabase::CURRENT_SCHEMA_VERSION);
        std::cout << "  -> PASSED: SQLite initialized in-memory with verified Schema V2." << std::endl;
    }

    // Test 7: LocalDatabase repeated initialization idempotency
    {
        std::cout << "[Test 7] Testing repeated initialization idempotency..." << std::endl;
        LocalDatabase db;
        assert(db.initialize(":memory:"));
        assert(db.initialize(":memory:")); // Repeated call must not crash or corrupt
        assert(db.isOpen());
        assert(db.schemaVersion() == LocalDatabase::CURRENT_SCHEMA_VERSION);
        std::cout << "  -> PASSED: Repeated initialization is safe and idempotent." << std::endl;
    }

    // Test 8: App preferences persistence roundtrip
    {
        std::cout << "[Test 8] Testing key-value preferences roundtrip..." << std::endl;
        LocalDatabase db;
        db.initialize(":memory:");
        assert(db.setPreference("offline_region", "uttarakhand"));
        assert(db.setPreference("dark_mode", "true"));

        assert(db.getPreference("offline_region") == "uttarakhand");
        assert(db.getPreference("dark_mode") == "true");
        assert(db.getPreference("non_existent_key", "default_val") == "default_val");
        std::cout << "  -> PASSED: Preferences saved and retrieved correctly." << std::endl;
    }

    // Test 9: Session caching and restoration roundtrip
    {
        std::cout << "[Test 9] Testing session persistence roundtrip..." << std::endl;
        LocalDatabase db;
        db.initialize(":memory:");

        UserProfile profile;
        profile.uid = "usr_uttarakhand_101";
        profile.displayName = "Pooja Bisht";
        profile.email = "pooja@rescue.in";
        profile.phone = "+91-9988776655";
        profile.role = UserRole::CitizenUser;
        profile.isVerified = true;
        profile.lastLoginMs = 1700000000000;

        assert(db.saveSession(profile));

        auto loaded = db.loadSession();
        assert(loaded.has_value());
        assert(loaded->uid == "usr_uttarakhand_101");
        assert(loaded->displayName == "Pooja Bisht");
        assert(loaded->email == "pooja@rescue.in");
        assert(loaded->role == UserRole::CitizenUser);
        assert(loaded->isVerified == true);

        assert(db.clearSession());
        assert(!db.loadSession().has_value());
        std::cout << "  -> PASSED: Session cached, loaded, and cleared without data corruption." << std::endl;
    }

    // Test 10: AppState authoritative navigation routing and role boundaries
    {
        std::cout << "[Test 10] Testing AppState navigation shell & role boundary enforcement..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        db->initialize(":memory:");
        AppState state(db);

        assert(state.currentNavigation() == "Welcome");
        assert(state.activePortalName() == "Unauthenticated Area");

        // Unauthenticated cannot access UserHome or AdminDashboard
        assert(!state.navigateTo("UserHome"));
        assert(!state.navigateTo("AdminDashboard"));
        assert(state.navigateTo("Register"));
        assert(state.currentNavigation() == "Register");

        // Citizen Login
        assert(state.loginDevelopmentUser("Citizen User", "citizen@georescux.org"));
        assert(state.currentNavigation() == "UserHome");
        assert(state.activePortalName() == "Citizen User Portal");

        // Citizen can navigate across User Portal screens
        assert(state.navigateTo("UserMap"));
        assert(state.navigateTo("UserSafeRoute"));
        assert(state.navigateTo("UserAlerts"));
        assert(state.navigateTo("UserContacts"));
        assert(state.navigateTo("UserProfile"));
        assert(state.navigateTo("UserHelp"));

        // Citizen CANNOT enter Admin Portal screens
        assert(!state.navigateTo("AdminDashboard"));
        assert(!state.navigateTo("AdminMap"));
        assert(!state.navigateTo("AdminSos"));
        assert(!state.navigateTo("AdminVerification"));

        // Admin Login
        assert(state.loginDevelopmentAdmin("Commander Admin", "commander@georescux.org"));
        assert(state.currentNavigation() == "AdminDashboard");
        assert(state.activePortalName() == "Admin Command Portal");

        // Admin can navigate across Admin Portal screens
        assert(state.navigateTo("AdminMap"));
        assert(state.navigateTo("AdminSos"));
        assert(state.navigateTo("AdminIncidents"));
        assert(state.navigateTo("AdminHazards"));
        assert(state.navigateTo("AdminEvents"));
        assert(state.navigateTo("AdminRoutes"));
        assert(state.navigateTo("AdminUsers"));
        assert(state.navigateTo("AdminVerification"));
        assert(state.navigateTo("AdminAlerts"));

        // PrototypeLab is always accessible
        assert(state.navigateTo("PrototypeLab"));
        assert(state.currentNavigation() == "PrototypeLab");

        // Logout returns to Welcome
        state.logout();
        assert(state.currentNavigation() == "Welcome");
        std::cout << "  -> PASSED: AppState strictly enforces role boundaries across all portals." << std::endl;
    }

    // Test 11: Emergency record scaffolding table check
    {
        std::cout << "[Test 11] Testing Phase 3 emergency state scaffolding table..." << std::endl;
        LocalDatabase db;
        db.initialize(":memory:");
        bool ok = db.recordEmergencyScaffold("EMERGENCY_2026_001", "ACTIVE", 1700000000000, 30.3165, 78.0322);
        assert(ok);
        std::cout << "  -> PASSED: Emergency record scaffolding table accepts valid schema entries." << std::endl;
    }

    // Test 12: Cached Admin Session Trust Boundary Enforcement
    {
        std::cout << "[Test 12] Testing Cached Admin Session Trust Boundary..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        db->initialize(":memory:");

        // Simulate an admin session profile saved in SQLite
        UserProfile adminProfile;
        adminProfile.uid = "admin_cached_999";
        adminProfile.displayName = "Untrusted Admin";
        adminProfile.email = "untrusted.admin@georescux.org";
        adminProfile.phone = "+91-9999999999";
        adminProfile.role = UserRole::AuthorizedAdmin;
        adminProfile.isVerified = true;
        assert(db->saveSession(adminProfile));

        // Attempting to restore this session in SessionModel directly must fail
        SessionModel session;
        bool okRestore = session.restoreSession(adminProfile);
        assert(!okRestore);
        assert(session.sessionState() == SessionState::SessionError);
        assert(!session.isAuthenticated());
        assert(!session.isAdmin());

        // AppState initialization from persistence must purge the cached admin session
        AppState state(db);
        bool initialized = state.initializeSessionFromPersistence();
        assert(!initialized);
        assert(!db->loadSession().has_value()); // Admin cache MUST be purged!
        assert(state.session()->sessionState() == SessionState::Unauthenticated);
        assert(!state.session()->isAdmin());
        std::cout << "  -> PASSED: Cached Admin role rejected as proof of authority and purged." << std::endl;
    }

    // Test 13: Direct SessionModel logout clears database session reactively
    {
        std::cout << "[Test 13] Testing reactive database session clearing on logout..." << std::endl;
        auto db = std::make_shared<LocalDatabase>();
        db->initialize(":memory:");
        AppState state(db);

        assert(state.loginDevelopmentUser("Kavita", "kavita@rescue.org"));
        assert(db->loadSession().has_value());

        // Calling logout directly on the SessionModel (e.g. from QML sessionModel.logout())
        state.session()->logout();

        // Database cache must be cleared automatically via the connected signal
        assert(!db->loadSession().has_value());
        assert(state.currentNavigation() == "Welcome");
        std::cout << "  -> PASSED: Direct SessionModel logout reactively clears persistent database cache." << std::endl;
    }

    // Test 14: State-Role Invariant & Contradictory Transition Rejection
    {
        std::cout << "[Test 14] Testing state-role invariant enforcement..." << std::endl;
        SessionModel session;

        // Create an invalid profile where role does not match expected citizen user
        UserProfile invalidProfile;
        invalidProfile.uid = "inconsistent_user";
        invalidProfile.displayName = "Inconsistent";
        invalidProfile.email = "inc@test.com";
        invalidProfile.phone = "+91-1111111111";
        invalidProfile.role = UserRole::AuthorizedAdmin; // Admin role passed to Citizen login

        // Attempting to restore Admin as Citizen directly must fail
        bool ok = session.restoreSession(invalidProfile);
        assert(!ok);
        assert(session.sessionState() == SessionState::SessionError);
        assert(!session.isAuthenticated());
        assert(!session.isAdmin());
        std::cout << "  -> PASSED: Invariant violation rejected; session remains safe." << std::endl;
    }

    // Test 15: Qt SQL Thread-Affinity Guard on Cross-Thread Access
    {
        std::cout << "[Test 15] Testing Qt SQL thread-affinity guard..." << std::endl;
        LocalDatabase db;
        assert(db.initialize(":memory:"));

        bool threadCheckFailed = false;
        std::thread worker([&db, &threadCheckFailed]() {
            // Attempting to query LocalDatabase from a non-creator thread must be blocked
            bool ok = db.setPreference("thread_test", "fail");
            if (!ok && !db.lastError().isEmpty()) {
                threadCheckFailed = true; // Guard successfully caught and rejected cross-thread access!
            }
        });
        worker.join();

        assert(threadCheckFailed);
        // Creator thread can still safely operate
        assert(db.setPreference("main_thread", "success"));
        assert(db.getPreference("main_thread") == "success");
        std::cout << "  -> PASSED: Thread-affinity guard blocked cross-thread QSqlDatabase corruption." << std::endl;
    }

    // Test 16: Persistence Status Accuracy
    {
        std::cout << "[Test 16] Testing PersistenceStatus accuracy..." << std::endl;
        // Case A: Uninitialized database
        auto uninitDb = std::make_shared<LocalDatabase>();
        AppState stateA(uninitDb);
        assert(stateA.persistenceStatus() == PersistenceStatus::NotInitialized);
        assert(!stateA.isPersistenceReady());

        // Case B: Initialized database
        auto readyDb = std::make_shared<LocalDatabase>();
        readyDb->initialize(":memory:");
        AppState stateB(readyDb);
        assert(stateB.persistenceStatus() == PersistenceStatus::Ready);
        assert(stateB.isPersistenceReady());

        // Case C: Null database pointer
        AppState stateC(nullptr);
        assert(stateC.persistenceStatus() == PersistenceStatus::NotInitialized);
        assert(!stateC.isPersistenceReady());
        std::cout << "  -> PASSED: PersistenceStatus accurately distinguishes all database states." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 16 PHASE 2.1 FOUNDATION UNIT TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
