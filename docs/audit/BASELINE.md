# GeoRescuX — Phase 0: Baseline Inventory & Repository Audit

## 1. Executive Summary & Environment Baseline

- **Repository**: `C:\Users\sanid\Documents\GitHub\GeoRescuX`
- **Audit Date**: 21-10-2026
- **Git Branch**: `main`
- **Base Commit**: `b62a786f453a2e19bf00195440eeee0ccf794323`
- **Working Tree State**: Clean (0 uncommitted changes, 0 untracked build artifacts)
- **Primary Toolchains Detected**:
  - Android SDK: `C:\Users\sanid\AppData\Local\Android\Sdk` (compileSdk 35, buildTools 35.0.0, minSdk 24)
  - Android Gradle Plugin (AGP): `8.8.0` with Gradle `8.11.1`
  - Kotlin Compiler: `2.0.21` (JVM 11 target)
  - Native NDK & CMake: CMake `3.22.1` with NDK Clang (C++20 standard)
  - Qt Framework: `C:\Qt\6.11.1` with kits `mingw_64` (Desktop) and `android_arm64_v8a` (Android ARM64)
  - Desktop C++ Toolchain: MinGW GCC 13.1.0 64-bit with Ninja
  - Node.js Runtime: v20+ with Firebase Admin SDK 11+
  - Android Debug Bridge (ADB): `adb.exe` detected; **0 devices currently attached**

---

## 2. Directory Tree & Module Role Matrix

```
GeoRescuX/
├── app/                                       # Primary Kotlin Android Application (Production)
├── prototype_qt/                              # Native C++20 / Qt 6.11.1 Android & Desktop Prototype
├── backend/                                   # High-Throughput C++ Drogon Emergency Microservice
├── functions/                                 # Firebase Cloud Functions (Node.js)
├── georescux-admin-provision/                 # Out-of-Band Firebase Admin Claim Provisioning
├── docs/                                      # Multi-Tier Engineering & Architecture Documentation
│   ├── audit/                                 # Audit baselines, defect registers, limitations
│   ├── architecture/                          # OOP analysis, class reference, repository guides
│   ├── testing/                               # Test reports, APK hashes, build logs
│   ├── security/                              # Security reviews, Firebase rules, auth analysis
│   └── development/                           # Build, setup, and toolchain instructions
├── output/                                    # Regional Bundled Map Datasets (Himalayas & Plains)
├── source/                                    # Raw OpenStreetMap Data Sources & Boundaries
├── tools/                                     # Offline Graph Compilers & Map Converters
├── database.rules.json                        # Firebase Realtime Database Security Rules
├── firebase.json                              # Firebase Project Deployment Configuration
├── build.gradle.kts                           # Root Gradle Project Build Script
└── settings.gradle.kts                        # Gradle Module Inclusions
```

---

## 3. Subsystem Breakdown by Technology

| Subsystem | Primary Language | Framework / Toolchain | Runtime Role | Verification Status |
| :--- | :--- | :--- | :--- | :--- |
| **Android Client** | Kotlin (60%) / C++ (40%) | Android SDK 35, Jetpack, OSMDroid, MapsForge | Citizen SOS, offline route navigation, Bluetooth mesh | **PASSED (Unit + Build)** |
| **Qt Prototype** | C++20 / QML | Qt 6.11.1, CMake, MinGW / Android Clang | Cross-platform tactical emergency client | **PASSED (CTest + APK)** |
| **Backend API** | C++20 | Drogon Web Framework, SQLite3 | Disaster physics simulations, incident clustering | **SOURCE INSPECTED** |
| **Admin Provisioning** | JavaScript | Node.js, Firebase Admin SDK | Cryptographic minting of `admin: true` claims | **SOURCE INSPECTED** |
| **Cloud Functions** | JavaScript | Node.js, Firebase Functions SDK | Serverless claim verification & admin endpoints | **DEFECT CONFIRMED** |

---

## 4. Baseline Build & Test Execution Summary

### A. Kotlin Android Production Target
- **Command**: `.\gradlew.bat testDebugUnitTest`
- **Result**: `BUILD SUCCESSFUL` (25 actionable tasks executed, 0 failures)
- **Release Build**: `.\gradlew.bat assembleRelease`
- **Result**: `BUILD SUCCESSFUL` (59 actionable tasks executed)
- **Output Artifact**: `app/build/outputs/apk/release/app-release-unsigned.apk` (36,681,165 bytes)
- **SHA-256**: `E1C5CA9FAB2674EC932B910D0C6A4F786F578E1D0BE9A6991EB1B0FFA91B1EFD`

### B. Qt C++ Prototype Target
- **Command**: `ctest --test-dir prototype_qt/build_desktop --output-on-failure`
- **Result**: `100% tests passed` (6 of 6 suites passed in 6.21 seconds)
  - `GeodesicMathTest`: PASSED
  - `FoundationTest`: PASSED
  - `EmergencyLifecycleTest`: PASSED
  - `MapPackageTest`: PASSED
  - `RoutingTest`: PASSED
  - `LocationTest`: PASSED
- **Android ARM64 Build Artifact**: `prototype_qt/build_android_arm64/android-build/appGeoRescuXQtPrototype.apk` (35,361,673 bytes)
- **SHA-256**: `87EE20C1A63B9B4C9D2BEC9BE6FE70E7BA175765182D7AB860B2677F393DE0C5`

---

## 5. Baseline Environmental Limitations

1. **Physical Hardware Disconnection**: `adb devices` indicates no active physical device or running emulator. All on-device RF testing (BLE mesh packet transmission between two phones) and live GNSS satellite lock tests are blocked in this environment.
2. **Cloud Write Prohibition**: Live deployment of Firebase security rules, cloud functions, and live writes to production databases are strictly disallowed to maintain environment safety.
