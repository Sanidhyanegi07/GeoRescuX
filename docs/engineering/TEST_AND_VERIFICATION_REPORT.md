# GeoRescuX — Test Execution & Verification Report

## 1. Test Strategy & Verification Hierarchy

GeoRescuX uses a rigorous multi-tier verification process spanning unit tests, integration tests, native C++ CTest suites, static analysis, and multiplatform APK artifact verification:

```
[Layer 1: Unit & Domain Tests]      --> [Layer 2: Native CTest Suites]      --> [Layer 3: Build & APK Artifacts]
• Kotlin JVM Unit Tests                 • MinGW 64-bit Desktop Tests            • assembleRelease (Production)
• SafeHavenFallback Routing Tests       • Geodesic & Mathematical Tests         • assembleDebug (Development)
• SOS Serialization Tests               • SQLite Repository Tests               • Qt Android ARM64 APK
• Role Resolution & Claims Tests        • Location & Emergency Service Tests    • SHA-256 Checksums
```

---

## 2. Test Execution Results

### A. Android Kotlin Unit Tests
- **Command**: `.\gradlew.bat testDebugUnitTest`
- **Working Directory**: `c:\Users\sanid\Documents\GitHub\GeoRescuX`
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Execution Time**: 3 minutes 49 seconds
- **Summary**: 25 actionable tasks executed; 0 failures; 0 errors.
- **Key Test Suites Executed**:
  1. `UttarakhandRegionGraphTest`: Validates parsing and route discovery on real-world OSM topology graph asset (~18 MB).
  2. `AStarPathfinderTest`: Verifies A* cost optimality, blocked edge avoidance, and hazard penalty addition.
  3. `SafeHavenFallbackTest`: Asserts deterministic fallback to the nearest reachable emergency shelter when the primary destination is blocked.
  4. `SosEmergencyJsonTest`: Verifies bidirectional JSON serialization and confirms note sanitization against literal `"null"`.
  5. `SyncStateStoreTest`: Tests durable tracking of pending SOS and contact records across simulated app restarts.
  6. `MapRegionCatalogTest`: Confirms region catalog integrity, boundary definitions, and versioning.

---

### B. C++20 / Qt 6.11.1 CTest Suites
- **Command**: `ctest --test-dir prototype_qt/build_desktop --output-on-failure`
- **Compiler / Toolchain**: MinGW 13.1.0 64-bit / CMake 3.22.1 / Qt 6.11.1
- **Exit Code**: `0` (100% tests passed)
- **Execution Time**: 6.21 seconds
- **Suite Breakdown**:

| Test # | Test Name | Target Executable | Duration | Result | Features Verified |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **1** | `GeodesicMathTest` | `tst_geodesic.exe` | 0.27s | **PASSED** | Haversine distance, Vincenty ellipsoidal geodesic calculations, bearing angles. |
| **2** | `FoundationTest` | `tst_foundation.exe` | 2.09s | **PASSED** | Bloom filter bit-saturation, packet codecs, triage classification, HMAC-SHA256 authentication. |
| **3** | `EmergencyLifecycleTest` | `tst_emergency.exe` | 1.07s | **PASSED** | State machine countdown, single-active invariant, SQLite transactional persistence, completion timestamps. |
| **4** | `MapPackageTest` | `tst_map_package.exe` | 0.47s | **PASSED** | Regional manifest parsing, boundary GeoJSON validation, file cataloging. |
| **5** | `RoutingTest` | `tst_routing.exe` | 0.41s | **PASSED** | C++ A* pathfinder, turn penalties, Contraction Hierarchies, multi-modal evacuation routing. |
| **6** | `LocationTest` | `tst_location.exe` | 1.87s | **PASSED** | `FakeLocationProvider` speed interpolation, heading calculation, accuracy degradation, permission state. |

---

### C. Release Build Verification
- **Command**: `.\gradlew.bat assembleRelease`
- **Target**: Android Production Release APK
- **Exit Code**: `0` (BUILD SUCCESSFUL)
- **Execution Time**: 1 minute 24 seconds
- **Tasks**: 59 actionable tasks executed; 33 up-to-date; 26 executed.
- **Verification**: Android NDK CMake compiled shared native libraries (`libgeorescux_routing.so`) across `arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`. Kotlin resources and merged XML layouts compiled cleanly.

---

## 3. Master APK Artifact Register

The table below lists all generated APK binaries currently present in the repository, with exact byte sizes, build timestamps, and cryptographic SHA-256 hashes:

| Application Target | Relative Path | Size (Bytes) | Build Timestamp | SHA-256 Hash |
| :--- | :--- | :---: | :---: | :--- |
| **Kotlin Production (Release)** | `app\build\outputs\apk\release\app-release-unsigned.apk` | 36,681,165 | 21-10-2026 12:35:19 | `E1C5CA9FAB2674EC932B910D0C6A4F786F578E1D0BE9A6991EB1B0FFA91B1EFD` |
| **Kotlin Production (Debug)** | `app\build\outputs\apk\debug\app-debug.apk` | 41,491,530 | 21-10-2026 00:49:01 | `B1812FDBCFC712FE1A5162F54EDFD1C5BB9B28682DE04FE7818A613AFEB646F9` |
| **Qt Prototype (ARM64 Release)** | `prototype_qt\build_android_arm64\android-build\appGeoRescuXQtPrototype.apk` | 35,361,673 | 21-10-2026 00:35:30 | `87EE20C1A63B9B4C9D2BEC9BE6FE70E7BA175765182D7AB860B2677F393DE0C5` |
| **Qt Prototype (ARM64 Debug)** | `prototype_qt\build_android_arm64\android-build\build\outputs\apk\debug\android-build-debug.apk` | 33,464,089 | 21-10-2026 00:55:13 | `A1840A7811EFB30D54A7E2F2922B62233FD77981E8EEE7638289096F26F91127` |

---

## 4. Environment & Verification Scope Disclaimers

In strict accordance with engineering auditing standards, the exact verification boundary for each tier is documented below:

| Verification Tier | Method / Tools | Status | Notes & Constraints |
| :--- | :--- | :--- | :--- |
| **Source Inspection** | Static Code Analysis, AST Review | **VERIFIED** | All Kotlin, C++, QML, and XML files inspected. |
| **Unit & CTest Execution** | JUnit, Gradle, CTest, MinGW | **VERIFIED** | 100% automated test pass rate across Kotlin and C++. |
| **Binary Compilation** | Gradle AGP, CMake, Clang | **VERIFIED** | Clean compilation of release and debug APKs. |
| **Physical RF (BLE) Testing** | Multi-Device Bluetooth Sniffer | **BLOCKED** | Requires physical Android devices in RF-controlled lab. |
| **Live Satellite GPS Testing** | Real Hardware NMEA/GNSS Antenna | **BLOCKED** | Requires outdoor open-sky physical field trials. |
| **Live Cloud Sync Testing** | Production Firebase Database | **BLOCKED** | Live cloud writes prohibited during offline audit. |
