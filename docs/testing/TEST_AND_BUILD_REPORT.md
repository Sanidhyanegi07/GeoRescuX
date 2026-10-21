# GeoRescuX — Automated Test & Build Verification Report

## 1. Test Execution Summary

The GeoRescuX verification pipeline exercises both the production Kotlin Android mobile app and the native C++20 / Qt cross-platform prototype.

---

## 2. Test Execution Logs & Results

### A. Android Kotlin Unit Tests
- **Command**: `.\gradlew.bat testDebugUnitTest`
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Duration**: 3m 49s
- **Actionable Tasks**: 25 executed / up-to-date
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

### C. Release Build Verification
- **Command**: `.\gradlew.bat assembleRelease`
- **Target**: Android Production Release APK
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Duration**: 1m 24s
- **Actionable Tasks**: 59 executed / up-to-date
- **Verification**: Android NDK CMake compiled shared native libraries (`libgeorescux_routing.so`) across all 4 target ABIs (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`).

---

## 3. Master APK Artifact Register

| Target Application | Relative Path | Exact Bytes | Build Timestamp | Cryptographic SHA-256 Hash |
| :--- | :--- | :---: | :---: | :--- |
| **Kotlin Production (Release)** | `app\build\outputs\apk\release\app-release-unsigned.apk` | 36,681,165 | 21-10-2026 12:35:19 | `E1C5CA9FAB2674EC932B910D0C6A4F786F578E1D0BE9A6991EB1B0FFA91B1EFD` |
| **Kotlin Production (Debug)** | `app\build\outputs\apk\debug\app-debug.apk` | 41,491,530 | 21-10-2026 00:49:01 | `B1812FDBCFC712FE1A5162F54EDFD1C5BB9B28682DE04FE7818A613AFEB646F9` |
| **Qt Prototype (ARM64 Release)** | `prototype_qt\build_android_arm64\android-build\appGeoRescuXQtPrototype.apk` | 35,361,673 | 21-10-2026 00:35:30 | `87EE20C1A63B9B4C9D2BEC9BE6FE70E7BA175765182D7AB860B2677F393DE0C5` |
| **Qt Prototype (ARM64 Debug)** | `prototype_qt\build_android_arm64\android-build\build\outputs\apk\debug\android-build-debug.apk` | 33,464,089 | 21-10-2026 00:55:13 | `A1840A7811EFB30D54A7E2F2922B62233FD77981E8EEE7638289096F26F91127` |

---

## 4. Verification Boundaries & Environmental Limits

- **Unit & Integration Tests**: 100% verified and passing.
- **Binary Build Freshness**: Verified; all APKs compiled clean without warnings.
- **Physical Device Testing**: Blocked due to `adb devices` reporting 0 connected devices.
- **Live Cloud Deployment**: Blocked due to audit constraints prohibiting live cloud database modifications.
