# GeoRescuX: Final Release Verification Evidence Log

This document records the empirical baseline data, build artifacts, test outputs, and runtime execution metrics gathered during the final release candidate verification of GeoRescuX. All sensitive authentication tokens, service account credentials, private cryptographic keys, and personally identifiable coordinates have been omitted or sanitized.

---

### 1. Repository & Baseline Integrity

- **Repository Root:** `C:\Users\sanid\Documents\GitHub\GeoRescuX`
- **Active Branch:** `main`
- **Baseline Git Lineage:**
  - **Initial Mesh Baseline Commit:** `546c7690d74e2a89bff2f47164d66d767c85f849` (`feat: add cross-transport relay bridge and BLE mesh routing engine`)
  - **Cold-Start Fix & Build Baseline Commit:** `a05fea27f2511f0a39dfc53f6ffb494c5dc759d5` (`feat: implement role-based routing coordinator and main activity entry point`)
  - **Documentation Baseline Commit (HEAD):** `abb0ad7f707691ebd15b6c0ca09b00d2ffc1467a` (`docs: add final release documentation, demonstration checklist, and viva guide`)
- **Git Working Tree Status:**
  - **Modified Tracked Files:** Strictly limited to the 5 documentation files in `docs/final-release/`:
    1. `docs/final-release/FINAL_PROJECT_REPORT.md`
    2. `docs/final-release/FEATURE_VERIFICATION_MATRIX.md`
    3. `docs/final-release/DEMONSTRATION_CHECKLIST.md`
    4. `docs/final-release/KNOWN_LIMITATIONS.md`
    5. `docs/final-release/RELEASE_EVIDENCE.md`
  - **Untracked Files:** **ZERO.** No new temporary files, build outputs, or scripts exist in the repository root.
  - **Staged / Committed / Pushed Changes:** **ZERO.** No Git history alteration, no staging (`git add`), no resets, and no remote pushes (`git push`) were performed. All working-tree documentation refinements remain uncommitted per preservation rules.
- **Verification Qualification:** Branch, HEAD, commit lineage, and working-tree status are recorded repository observations from the active Git workspace; they are not independently verifiable from a standalone documentation ZIP lacking `.git/` metadata.

---

### 2. Automated Unit Test Verification

- **Execution Command:** `.\gradlew.bat testDebugUnitTest --rerun-tasks`
- **Final Gradle Status:** `BUILD SUCCESSFUL in 5m 7s` (25 actionable tasks executed)
- **Exit Code:** `0`
- **Report Verification:** Parsed directly from all 74 XML files in `app/build/test-results/testDebugUnitTest/`
- **Aggregated Results:**
  - **Total Test Suites:** 74
  - **Total Tests Executed:** **593**
  - **Failures:** **0**
  - **Errors:** **0**
  - **Skipped / Ignored:** **0**
  - **Pass Rate:** **100.0%**
- **Verification Qualification:** The 593 passing unit tests (0 failures, 0 errors, 0 skipped across 74 suites) are recorded execution results from the verified build environment. They are not independently reproducible from the documentation ZIP alone, as raw XML test reports (`app/build/test-results/testDebugUnitTest/`) are excluded from documentation packages.

#### Target Subsystems & Security Tests:
1. **`RoleRoutingCoordinatorTest`** (`TEST-com.example.georescux.ui.main.RoleRoutingCoordinatorTest.xml`): 11/11 PASSED
   - `1 successful USER resolution routes to Home` — **PASSED** (0.003s)
   - `2 successful ADMIN resolution routes to AdminDashboard` — **PASSED** (0.002s)
   - `3 missing admin claim routes to Home` — **PASSED** (0.010s)
   - `4 token retrieval failure fails closed to UNKNOWN failed state` — **PASSED** (0.003s)
   - `4b exception during token retrieval fails closed securely` — **PASSED** (0.007s)
   - `5 timeout fails closed to timeout failure state` — **PASSED** (0.007s)
   - `6 coroutine cancellation rethrows CancellationException` — **PASSED** (0.006s)
   - `7 retry after connectivity returns succeeds and routes to Admin` — **PASSED** (0.003s)
   - `8 UNKNOWN state cannot access AdminDashboard` — **PASSED** (0.003s)
   - `9 duplicate navigation is prevented idempotent hasNavigated` — **PASSED** (0.003s)
   - `unauthenticated user routes immediately to Login` — **PASSED** (0.003s)
2. **`AdminAuthGuardTest`** (`TEST-com.example.georescux.domain.auth.AdminAuthGuardTest.xml`): 6/6 PASSED
   - `isAuthorizedAdmin returns true when role is ADMIN` — **PASSED**
   - `isAuthorizedAdmin returns false when role is USER` — **PASSED**
   - `isAuthorizedAdmin returns false when role is UNKNOWN` — **PASSED**
   - `isAuthorizedAdmin fails closed and returns false on exception` — **PASSED**
   - `isAuthorizedAdmin fails closed and returns false on timeout` — **PASSED**
   - `isAuthorizedAdmin rethrows CancellationException` — **PASSED**
3. **`SQLiteSeenEventStoreEscapeTest`** (`TEST-com.example.georescux.data.relay.SQLiteSeenEventStoreEscapeTest.xml`): 5/5 PASSED
   - Wildcard escaping (`%`, `_`, `\`) tested and verified for SQLite `LIKE ? ESCAPE '\'`.
4. **`SyncingSosRepositoryTest`**: 25/25 PASSED
   - Validates that local operations succeed first, failed remote uploads retain pending sync markers, and successful sync clears markers.
5. **Real Firebase RTDB Emulator Security Rules** (`run_emulator_rules_tests.js`): **60/60 PASSED (100%)**
   - Verified owner cannot overwrite or demote `RESOLVED` status back to `ACTIVE` or `COMPLETED` at either child `status` or parent `$alertId` path (`setValue` or `updateChildren`).
   - Verified owner cannot remove a `RESOLVED` record (`DELETE`).
   - Verified owner can update permitted telemetry and timestamps while preserving `RESOLVED`.
   - Verified admin can resolve emergency but cannot modify victim coordinates, note, or start timestamp.
   - Verified ordinary user cannot read `/events` or elevate `isVerified`.
6. **Firebase Cloud Functions Security Gatekeeper** (`functions/test/admin_security.test.js`): **15/15 PASSED (100%)**
   - Verified non-admin callers attempting privilege escalation receive `permission-denied`.
   - Verified token custom claims merge and revoke safely.

---

### 3. Application Build & Artifact Verification

- **Execution Command:** `.\gradlew.bat assembleDebug`
- **Final Gradle Status:** `BUILD SUCCESSFUL in 9s` (45 actionable tasks: 11 executed, 34 up-to-date)
- **Exit Code:** `0`
- **Artifact Path:** `app/build/outputs/apk/debug/app-debug.apk`
- **File Size:** `42,750,105` bytes
- **Build Timestamp:** `16-11-2026 00:26:53` (Local Time)
- **SHA-256 Checksum:** `56890a9faf312c16c2f6de5ee2c78518152f616e1cef6a3ccb5a918cb664be54`
- **Archive Verification:**
  - Verified presence of `res/layout/activity_main.xml` within the APK zip structure.
  - Verified presence of native C++20 shared libraries across all 4 ABIs: `lib/arm64-v8a/libgeorescux_routing.so`, `lib/armeabi-v7a/libgeorescux_routing.so`, `lib/x86/libgeorescux_routing.so`, `lib/x86_64/libgeorescux_routing.so`.
  - Verified presence of bundled offline assets: `assets/route_graph_uttarakhand`, `assets/maps/uttarakhand-state.map`, `assets/maps/sample-region/graph.json`, `assets/maps/sample-region/tiles.sqlite`.
  - Verified DEX output partitions (`classes.dex` through `classes18.dex`).
- **Verification Qualification:** The APK size (42,750,105 bytes) and SHA-256 checksum (`56890a9faf312c16c2f6de5ee2c78518152f616e1cef6a3ccb5a918cb664be54`) are recorded artifact metadata from the local Gradle build. They do not constitute proof that the binary APK is bundled inside the documentation-only distribution archive.

---

### 4. Target Test Environment Profile

- **Device Identifier:** `emulator-5554`
- **Device Profile:** Google Pixel 6
- **Architecture / ABI:** `x86_64,arm64-v8a`
- **System Image:** Android 14/15 16k page size preview image (`google_apis_ps16k`)
- **Screen Resolution:** `1080 x 2400` @ 420 dpi
- **Package Installed:** `com.example.georescux`
- **Installed Attributes:** `versionCode=1`, `minSdk=24`, `targetSdk=35`, `versionName=1.0`

---

### 5. Runtime Cold-Start Launch Telemetry

- **Trigger Method:**
  ```powershell
  adb shell am force-stop com.example.georescux
  adb logcat -c
  adb shell am start -W -n com.example.georescux/.MainActivity
  ```
- **Observed Metrics:**
  - `Status: ok`
  - `LaunchState: COLD`
  - `Activity: com.example.georescux/.MainActivity`
  - `TotalTime: 14716ms`
  - `WaitTime: 14720ms`
- **Verification Qualification:** The 14.716-second cold-start launch duration is recorded runtime telemetry from virtual hardware (`emulator-5554`), reflecting emulator virtualization and Android OS class verification overhead.
- **Logcat Activity Transition:**
  ```
  11-15 22:55:33.229 ActivityTaskManager: START u0 {cmp=com.example.georescux/.MainActivity} ...
  11-15 22:55:47.763 ActivityTaskManager: Displayed com.example.georescux/.MainActivity for user 0: +14s716ms
  11-15 22:55:48.061 ActivityTaskManager: START u0 {cmp=com.example.georescux/.ui.home.HomeActivity} ...
  11-15 22:55:49.697 ActivityTaskManager: Displayed com.example.georescux/.ui.home.HomeActivity for user 0: +1s653ms
  ```
- **Final Focused Window:**
  `mCurrentFocus=Window{... u0 com.example.georescux/com.example.georescux.ui.home.HomeActivity}`
- **Anomaly Detection:** Zero ANRs, zero crashes, zero duplicate activity starts.

---

### 6. SOS Lifecycle & Persistence Smoke Test Telemetry

#### Step 1: Emergency Initiation
- UI input on `buttonSos` at bounds `[53, 566][522, 949]`.
- Screen transitioned to `SosActivity`.
- UI displayed: `"SOS in Progress"`, `"Started at 22:56"`, and `"Safe Route Pro Active"`.
- On-disk inspection (`shared_prefs/sos_store.xml`):
  - Emergency active: `startedAtMs: 1794763614435`, `stoppedAtMs: null`.
  - Location status: `ACQUIRED`, sequence counter: `4`.
  - Historical records: 4 pre-existing records intact.

#### Step 2: Emergency Completion
- UI input on `buttonStopEmergency` at center coordinates `(540, 1908)`.
- `SosActivity` dismissed gracefully; focus returned to `HomeActivity`.
- On-disk inspection (`shared_prefs/sos_store.xml`):
  - `active_emergency` key completely removed.
  - Emergency appended to `sos_history` with terminal timestamp: `stoppedAtMs: 1794763650545`, `sequence: 6`.
  - Total historical emergencies: 5 records.

#### Step 3: Process Termination & Resurrection Guard
- Executed `adb shell am force-stop com.example.georescux`.
- Relaunched application: `adb shell am start -W -n com.example.georescux/.MainActivity`.
- Measured launch: `COLD` start in `5960ms`, landed on `HomeActivity`.
- On-disk inspection (`shared_prefs/sos_store.xml`):
  - `active_emergency` key remained absent.
  - Zero false-alarm banners displayed on dashboard.
  - All 5 historical records preserved.

---

### 7. Explicit Scope & Verification Boundaries

1. **Unit Testing vs Physical RF:** The 582 unit tests validate algorithmic logic, deduplication math, packet codecs, and state machines. They do not substitute for physical multi-device BLE/Nearby testing between hardware smartphones.
2. **Virtual vs Real GPS:** Coordinates recorded during tests were generated by Android Emulator's mock fused location provider and do not represent physical satellite RF reception.
3. **Local Store vs Cloud Readback:** Local non-volatile persistence in `sos_store.xml` is verified. Independent remote cloud query via external REST endpoints directly against `/sos_alerts/{uid}/{alertId}` was not executed.
4. **Relay Cryptographic Authentication Status:** ECDSA signing primitives (`EventSigner.kt`, `EventAuthenticator.kt`) exist in code and pass unit tests, but production `RelayEngine` in `GeoRescuXApplication.kt` is initialized with default `authenticator = null`, and `GeoRescueBlePacket` contains no signature field. Cryptographic mesh authentication is therefore implemented in code but unenforced in the production relay path.
5. **Zero Code Pollution:** No application source code, manifests, Firebase rules, or Git commit histories were modified or polluted during this documentation phase.

---

### 8. Evidence Packaging and Independent Reproducibility

This subsection explicitly clarifies what is included versus excluded in the final release documentation distribution archive, and provides the exact procedure required for independent reproduction:

#### A. Files Physically Included in Documentation Archive
The standalone documentation ZIP includes only the final release Markdown documentation files located in `docs/final-release/`:
1. `FINAL_PROJECT_REPORT.md` (System architecture, technical specifications, and verification summary)
2. `FEATURE_VERIFICATION_MATRIX.md` (Detailed feature-by-feature verification status, evidence, and limitation matrix)
3. `DEMONSTRATION_CHECKLIST.md` (Step-by-step evaluator walkthrough and 20 comprehensive technical viva Q&As)
4. `KNOWN_LIMITATIONS.md` (Engineering risk analysis, unverified subsystems, and known limitations)
5. `RELEASE_EVIDENCE.md` (This empirical verification evidence log)

#### B. Artifacts Excluded from Documentation Archive
To keep documentation archives portable, lightweight, and text-focused, raw binary and build artifacts are excluded:
- **Raw Test Report XMLs:** The 74 individual XML result files in `app/build/test-results/testDebugUnitTest/` are not bundled in the documentation ZIP.
- **Compiled Binary APK:** The physical `app-debug.apk` binary (42,750,105 bytes) is not bundled in the documentation ZIP.
- **Git Repository Metadata:** The `.git/` version control tree is excluded from standalone documentation ZIP exports.

#### C. Requirements for Independent Evaluator Reproduction
An external auditor or examiner wishing to independently reproduce all recorded execution results and artifact metadata should follow these steps using the complete GeoRescuX repository source checkout:
1. **Source Code Checkout:**
   Clone or extract the repository at commit `abb0ad7` (or current release candidate checkout).
2. **Prerequisites:**
   - JDK 17 (Java Development Kit 17.0.x).
   - Android SDK platform-tools and build-tools (minSdk 24, compileSdk 35, targetSdk 35).
   - Android NDK (r25+ with CMake supporting C++20 for `libgeorescux_routing.so`).
3. **Reproduce Automated Unit Test Suite (593 Tests):**
   Run from the repository root:
   ```powershell
   .\gradlew.bat testDebugUnitTest --rerun-tasks
   ```
   Verify that Gradle reports `BUILD SUCCESSFUL`, generates all 74 XML files in `app/build/test-results/testDebugUnitTest/`, and confirms:
   - Total Tests: **593**
   - Failures: **0**, Errors: **0**, Skipped: **0**
   - Specifically verify `RoleRoutingCoordinatorTest` (11/11 passed), `AdminAuthGuardTest` (6/6 passed), and `SQLiteSeenEventStoreEscapeTest` (5/5 passed).
4. **Reproduce Debug APK Compilation & Checksum:**
   Run from the repository root:
   ```powershell
   .\gradlew.bat assembleDebug
   ```
   Inspect the output at `app/build/outputs/apk/debug/app-debug.apk`. Calculate its SHA-256 hash:
   ```powershell
   Get-FileHash app/build/outputs/apk/debug/app-debug.apk -Algorithm SHA256
   ```
   Confirm file size (`42,750,105` bytes) and SHA-256 hash (`56890a9faf312c16c2f6de5ee2c78518152f616e1cef6a3ccb5a918cb664be54`).
5. **Reproduce Emulator Cold-Start Runtime Telemetry:**
   Launch an Android Virtual Device (AVD) running Google Pixel 6 (API Level 34/35) and execute:
   ```powershell
   adb install -r -d app/build/outputs/apk/debug/app-debug.apk
   adb shell am force-stop com.example.georescux
   adb logcat -c
   adb shell am start -W -n com.example.georescux/.MainActivity
   ```
   Observe the immediate branded loading screen (`activity_main.xml`), asynchronous role resolution with bounded 3000ms timeout, and cold-start launch duration.

---
*Evidence log certified against GeoRescuX release candidate artifact `app-debug.apk`.*
