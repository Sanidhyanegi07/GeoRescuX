# GeoRescuX: Final Release Verification Evidence Log

This document records the empirical baseline data, build artifacts, test outputs, and runtime execution metrics gathered during the final release candidate verification of GeoRescuX. All sensitive authentication tokens, service account credentials, private cryptographic keys, and personally identifiable coordinates have been omitted or sanitized.

---

### 1. Repository & Baseline Integrity

- **Repository Root:** `C:\Users\sanid\Documents\GitHub\GeoRescuX`
- **Active Branch:** `main`
- **Baseline Git Commit:** `546c7690d74e2a89bff2f47164d66d767c85f849` (HEAD at `a05fea27f2511f0a39dfc53f6ffb494c5dc759d5`)
- **Git Commit Timestamp:** Fri Nov 13 2026
- **Git Working Tree Status:**
  - **Tracked Modified Files:**
    1. `app/src/main/kotlin/com/example/georescux/MainActivity.kt` (Immediate layout rendering + asynchronous bounded role router)
    2. `app/src/main/res/values/strings.xml` (Branded role verification and recovery string resources)
    3. `output/haryana/manifest.json` (Offline routing timestamp from Gradle test task)
    4. `output/himachal_pradesh/manifest.json` (Offline routing timestamp from Gradle test task)
    5. `output/uttar_pradesh/manifest.json` (Offline routing timestamp from Gradle test task)
    6. `output/uttarakhand/manifest.json` (Offline routing timestamp from Gradle test task)
  - **New Untracked Files:**
    1. `app/src/main/kotlin/com/example/georescux/ui/main/RoleRoutingCoordinator.kt`
    2. `app/src/main/res/layout/activity_main.xml`
    3. `app/src/test/kotlin/com/example/georescux/ui/main/RoleRoutingCoordinatorTest.kt`
    4. `docs/final-release/FINAL_PROJECT_REPORT.md`
    5. `docs/final-release/FEATURE_VERIFICATION_MATRIX.md`
    6. `docs/final-release/DEMONSTRATION_CHECKLIST.md`
    7. `docs/final-release/KNOWN_LIMITATIONS.md`
    8. `docs/final-release/RELEASE_EVIDENCE.md`
  - **Staged / Committed / Pushed Changes:** **ZERO.** No Git history alteration, no staging, no resets, and no remote pushes were performed.

---

### 2. Automated Unit Test Verification

- **Execution Command:** `.\gradlew.bat testDebugUnitTest --rerun-tasks`
- **Final Gradle Status:** `BUILD SUCCESSFUL in 7m 4s` (25 actionable tasks executed)
- **Exit Code:** `0`
- **Report Verification:** Parsed directly from all 73 XML files in `app/build/test-results/testDebugUnitTest/`
- **Aggregated Results:**
  - **Total Test Suites:** 73
  - **Total Tests Executed:** **582**
  - **Failures:** **0**
  - **Errors:** **0**
  - **Skipped / Ignored:** **0**
  - **Pass Rate:** **100.0%**

#### Target Subsystem: `RoleRoutingCoordinatorTest`
Report File: `TEST-com.example.georescux.ui.main.RoleRoutingCoordinatorTest.xml`
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

---

### 3. Application Build & Artifact Verification

- **Execution Command:** `.\gradlew.bat assembleDebug`
- **Final Gradle Status:** `BUILD SUCCESSFUL in 11s` (45 actionable tasks: 12 executed, 33 up-to-date)
- **Exit Code:** `0`
- **Artifact Path:** `app/build/outputs/apk/debug/app-debug.apk`
- **File Size:** `42,750,105` bytes
- **Build Timestamp:** `15-11-2026 21:54:49` (Local Time)
- **SHA-256 Checksum:** `4EE47BF18A994018D5C5C253743743370BEFB762C53428133773621151BCC838`
- **Archive Verification:**
  - Verified presence of `res/layout/activity_main.xml` within the APK zip structure.
  - Verified DEX output partitions (`classes.dex` through `classes18.dex`).

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
*Evidence log certified against GeoRescuX release candidate artifact `app-debug.apk`.*
