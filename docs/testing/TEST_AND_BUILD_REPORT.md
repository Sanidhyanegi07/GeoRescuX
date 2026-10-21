# GeoRescuX — Automated Test & Build Verification Report

## 1. Test Execution Summary

The GeoRescuX verification pipeline exercises both the production Kotlin Android mobile app and the native C++20 / Qt cross-platform prototype.

---

## 2. Test Execution Logs & Results

### A. Android Kotlin Unit Tests
- **Command**: `.\gradlew.bat testDebugUnitTest`
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Total Tests**: **452 unit tests passed (0 failures, 0 errors, 0 skipped)**
- **Actionable Tasks**: 25 executed clean
- **Failures**: 0

#### Test Suite Breakdown
1. **`UttarakhandRegionGraphTest`**: Parses bundled ~18 MB binary/JSON graph asset; asserts graph node count (>1,000 nodes), bidirectional edge connectivity, and valid coordinate bounds (30°N–31.5°N, 78°E–80°E).
2. **`AStarPathfinderTest`**: Tests path optimality against Dijkstra baseline; asserts blocked edges are excluded (cost = Infinity); asserts hazard penalties increase path weight appropriately.
3. **`SafeHavenFallbackTest`**: Simulates complete road blockage to target hospital; confirms router falls back deterministically to nearest reachable safe shelter.
4. **`SosEmergencyJsonTest`**: Tests bidirectional serialization; confirms note sanitization against literal `"null"`.
5. **`SyncStateStoreTest`**: Tests pending flag persistence across simulated process death.
6. **`MapRegionCatalogTest`**: Validates regional manifests and catalog metadata integrity.

---

### B. C++20 / Qt 6.11.1 CTest Suites
- **Command**: `ctest --test-dir prototype_qt/build_desktop --output-on-failure`
- **Compiler**: MinGW GCC 13.1.0 64-bit / Ninja / CMake 3.22.1
- **Exit Code**: `0` (100% tests passed)
- **Duration**: 6.21s
- **Results**:

| Test # | Suite Name | Duration | Status | Scope |
| :---: | :--- | :---: | :---: | :--- |
| **1** | `GeodesicMathTest` | 0.27s | **PASSED** | Haversine distance, Vincenty ellipsoidal geodesic, bearing calculations. |
| **2** | `FoundationTest` | 2.09s | **PASSED** | Bloom filter bit-saturation, packet codecs, START triage, HMAC-SHA256 crypto. |
| **3** | `EmergencyLifecycleTest` | 1.07s | **PASSED** | State machine countdown, single-active invariant, SQLite persistence. |
| **4** | `MapPackageTest` | 0.47s | **PASSED** | Manifest parser, boundary GeoJSON validation, catalog file queries. |
| **5** | `RoutingTest` | 0.41s | **PASSED** | C++ A* pathfinder, turn penalties, Contraction Hierarchies, multi-modal evacuation. |
| **6** | `LocationTest` | 1.87s | **PASSED** | `FakeLocationProvider` trajectory interpolation, heading, accuracy degradation. |

---

### C. Cloud Functions Security Unit Tests
- **Command**: `npm test` (in `functions/`)
- **Engine**: Node.js v26.7.0 `node:test` test runner
- **Exit Code**: `0`
- **Duration**: 158.5ms
- **Results**: **15/15 tests passed (100%)**
- **Scope**: Rigorous regression testing of `functions/index.js` preventing self-bootstrap privilege escalation (FINDING-SEC-01), enforcing admin caller verification, fail-closed error handling, claim merging, and status query ownership.

---

### D. Release Build Verification
- **Command**: `.\gradlew.bat assembleRelease` (Kotlin) & `.\gradlew.bat assembleRelease` (Qt Android)
- **Target**: Android Production Release APKs (Unsigned)
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Actionable Tasks**: 59 executed/up-to-date (Kotlin) | 47 executed/up-to-date (Qt)
- **Verification**: Android NDK CMake compiled shared native libraries across ABIs (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`) for Kotlin; Qt deployment packaged stripped native `libappGeoRescuXQtPrototype_arm64-v8a.so` (4,266,872 bytes).

---

## 3. Master APK Artifact Register

| Target Application | Relative Path | Exact Bytes | Build Timestamp | Cryptographic SHA-256 Hash |
| :--- | :--- | :---: | :---: | :--- |
| **Kotlin Production (Release)** | `app\build\outputs\apk\release\app-release-unsigned.apk` | 36,681,165 | 21-10-2026 13:50:58 | `9814B2A577AD6D040832D286B9880A5CFCBE904FB57416B93AD4BF2681A7800B` |
| **Kotlin Production (Debug)** | `app\build\outputs\apk\debug\app-debug.apk` | 41,491,530 | 21-10-2026 00:49:01 | `B1812FDBCFC712FE1A5162F54EDFD1C5BB9B28682DE04FE7818A613AFEB646F9` |
| **Qt Prototype (ARM64 Release)** | `prototype_qt\build_android_arm64\android-build\build\outputs\apk\release\android-build-release-unsigned.apk` | 29,478,897 | 21-10-2026 13:56:49 | `854E77F0B29BED969DE053E3B70F09F36539DFDE595F790BD67443E34BEA85C6` |
| **Qt Prototype (ARM64 Debug)** | `prototype_qt\build_android_arm64\android-build\build\outputs\apk\debug\android-build-debug.apk` | 33,464,089 | 21-10-2026 00:55:13 | `A1840A7811EFB30D54A7E2F2922B62233FD77981E8EEE7638289096F26F91127` |

---

## 4. Verification Boundaries & Environmental Limits

- **Unit & Integration Tests**: 100% verified and passing.
- **Binary Build Freshness**: Verified; all APKs compiled clean without warnings.
- **Physical Device Testing**: Blocked due to `adb devices` reporting 0 connected devices.
- **Live Cloud Deployment**: Blocked due to audit constraints prohibiting live cloud database modifications.
