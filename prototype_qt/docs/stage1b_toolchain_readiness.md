# GeoRescuX — Stage 1B: Qt Android Toolchain Readiness Audit Report

**Date:** October 19, 2026  
**Auditor:** Antigravity Agentic Toolchain Audit System  
**Workspace:** `C:\Users\sanid\Documents\GitHub\GeoRescuX`  
**Prototype Directory:** `prototype_qt/`  
**Report File:** `prototype_qt/docs/stage1b_toolchain_readiness.md`

---

## Executive Summary

This audit assesses the readiness of the isolated `prototype_qt/` sandbox for compiling and packaging an Android ARM64 (`arm64-v8a`) APK using the host Qt installation and local Android tooling.

**Readiness Verdict:** **NOT READY FOR ANDROID COMPILATION (BLOCKERS DETECTED)**  
While desktop Qt 6.11.1 compilation, C++20 GeodesicMath execution, and automated offscreen smoke testing pass with 100% success, the environment lacks the Qt 6.11.1 Android target libraries (`android_arm64_v8a`), a compatible JDK 21 runtime, Android SDK command-line tools (`sdkmanager`), and the officially tested NDK revision (r27c). Additionally, no ARM64 physical device or ARM64 emulator is currently attached.

Under the mandatory safety rules, **no software was downloaded, installed, or modified** during this audit.

---

## Section A: Current Repository Status & Protected Path Confirmation

1. **Git Working Tree:**
   - Active branch: `main` (synchronized with `origin/main`).
   - Working tree state: **100% clean**. No unstaged or staged modifications.
2. **Protection of Production Assets:**
   - Production directories verified untouched: `app/`, `backend/`, `functions/`, `georescux-admin-provision/`, `docs/`, `output/`, `source/`, `tools/`.
   - Production configuration verified untouched: `build.gradle.kts`, `settings.gradle.kts`, `gradle.properties`, `local.properties`, `database.rules.json`, `firebase.json`.
   - Production C++ routing code (`app/src/main/cpp/spatial/GeodesicMath.cpp` and `GeodesicMath.h`) is included read-only by the prototype build and has not been altered.
   - Production application identifier: `com.example.georescux` (unmodified).
   - Prototype sandbox identifier: `com.example.georescux.qtprototype` (fully isolated).
3. **System Integrity:**
   - No global Windows environment variables or system registry entries were modified.
   - No build caches or offline map datasets were cleared.

---

## Section B: Toolchain Inventory (Actual Paths and Detected Versions)

Every entry below was verified through actual filesystem inspection and command execution:

| Component | Path / Location | Version Detected | Status |
| :--- | :--- | :--- | :--- |
| **Qt Desktop** | `C:\Qt\6.11.1\mingw_64` | Qt 6.11.1 (MinGW 64-bit GCC 13.1.0) | **Installed & Healthy** |
| **Qt Android Target** | `C:\Qt\6.11.1\android_arm64_v8a` | None (`Test-Path` returned `False`) | **Missing (Blocker)** |
| **Qt Host Android Tools** | `C:\Qt\6.11.1\mingw_64\bin\androiddeployqt.exe` | 6.11.1 (`androiddeployqt` & `androiddeployqt6`) | **Installed** |
| **Qt SDK Tool** | `C:\Qt\Tools\sdktool\bin\sdktool.exe` | 6.7.2 / Qt Creator 20.0.1 tool | **Installed** |
| **Host System JDK** | `C:\Program Files\Java\jdk-26.0.2` | Oracle JDK 26.0.2 (Build 26.0.2+10-55) | **Incompatible (Need JDK 21)** |
| **Android Studio JBR** | `C:\Program Files\Android\Android Studio\jbr` | OpenJDK 25.0.3 (Build 25.0.3+-15898627) | **Incompatible (Need JDK 21)** |
| **Android SDK Root** | `C:\Users\sanid\AppData\Local\Android\Sdk` | Configured in `local.properties` | **Installed** |
| **Android SDK Platforms** | `...\Android\Sdk\platforms` | `android-35`, `android-37.0` | **Installed (API 35 available)** |
| **Android Build-Tools** | `...\Android\Sdk\build-tools` | `35.0.0`, `36.0.0` | **Installed** |
| **Android Command-line Tools** | `...\Android\Sdk\cmdline-tools` | None (`cmdline-tools` directory missing) | **Missing (`sdkmanager` absent)** |
| **Android NDK** | `...\Android\Sdk\ndk\27.0.12077973` | Revision `27.0.12077973` (Release: `r27`) | **Partially Incompatible (Need r27c)** |
| **Host CMake** | `C:\Qt\Tools\CMake_64\bin\cmake.exe` | CMake 3.30.5 | **Installed** |
| **SDK CMake** | `...\Android\Sdk\cmake\3.22.1\bin\cmake.exe`| CMake 3.22.1 | **Installed** |
| **Host Ninja** | `C:\Qt\Tools\Ninja\ninja.exe` | Ninja 1.12.1 | **Installed** |
| **Android Debug Bridge** | `...\Android\Sdk\platform-tools\adb.exe` | ADB version 1.0.41 (v37.0.1-15733141) | **Installed** |
| **Connected Devices** | `adb devices -l` | 0 devices attached | **Missing physical device** |
| **Configured AVDs** | `C:\Users\sanid\.android\avd\Pixel_6.avd` | `Pixel_6` (Target: `android-37.1`, ABI: `x86_64`)| **Incompatible ABI (`x86_64`)** |

---

## Section C: Qt 6.11.1 Android Compatibility Matrix Findings

Extracted directly from installed Qt 6.11.1 reference documentation (`C:\Qt\Docs\Qt-6.11.1\qtdoc`):

1. **Android Target Architectures:**
   - Supported: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`.
   - Initial Target ABI: `arm64-v8a` (64-bit ARM).
2. **API Levels:**
   - **Minimum Supported API:** API 28 (Android 9.0).  
     *(Reference automated test device: Huawei MediaPad M5 10, CMR-W09, Android 9, API 28).*
   - **Recommended Target SDK:** API 35 / API 36 (`platforms;android-36`, `build-tools;36.0.0`).
3. **Java Development Kit (JDK):**
   - **Required Version:** **JDK 21** (`<tr valign="top" class="even"><td><b>JDK</b></td><td>JDK 21</td></tr>`).
   - Gradle / AGP pairing: Gradle 9.3.1 and Android Gradle Plugin 9.0.0.
   - *Risk:* Host JDK 26 and JBR 25 exceed Gradle/AGP bytecode compatibility and will fail during APK packaging.
4. **Android NDK:**
   - **Officially Supported Version:** **NDK 27.2.12479018 (Release r27c)** with Clang 17.0.2.  
     *(Source: `C:\Qt\Docs\Qt-6.11.1\qtdoc\android.html:129`)*.
   - *Official Qt Note:* *"It's recommended that Qt apps use the same NDK version used for building the official Qt for Android libraries to avoid missing symbol errors."*
   - Current installed NDK is `27.0.12077973` (r27 initial release).

---

## Section D: Prototype Android Configuration Audit

1. **Minimum Android API Requirement:**
   - Prototype `AndroidConfig.cmake` sets `QT_ANDROID_MIN_SDK_VERSION "28"` and `QT_ANDROID_TARGET_SDK_VERSION "35"`.
   - Manifest comment notes API 28 minimum.
   - Main QML status badge indicates `Target: API 28+ (C++20)`.
   - **Verdict:** Fully compliant with Qt 6.11 minimum API requirements.
2. **Qt Version Compatibility in CMake:**
   - `prototype_qt/CMakeLists.txt` requests `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick)` and `qt_standard_project_setup(REQUIRES 6.8)`.
   - Compatible with Qt 6.11.1 (`6.11.1 >= 6.8`).
3. **C++20 Standard:**
   - `CMAKE_CXX_STANDARD 20` and `CMAKE_CXX_STANDARD_REQUIRED ON`.
   - `GeodesicMath.h` uses `<numbers>` and `std::numbers::pi`. Both MinGW GCC 13.1.0 and NDK r27/r27c Clang 17.0.2 fully support C++20 standard library math constants.
4. **Application ID & Sandbox Coexistence:**
   - Package name: `com.example.georescux.qtprototype`.
   - App label: `GeoRescuX (Qt Prototype)` (defined in `res/values/strings.xml`).
   - Does not conflict with production `com.example.georescux`. Both can coexist simultaneously on the same test device.
5. **Android Manifest & Source Directory:**
   - `QT_ANDROID_PACKAGE_SOURCE_DIR` correctly references `${CMAKE_CURRENT_SOURCE_DIR}/platform/android`.
   - `AndroidManifest.xml` configures `org.qtproject.qt.android.bindings.QtActivity` with `android.app.lib_name` = `appGeoRescuXQtPrototype`.
   - `extractNativeLibs="true"` is set, ensuring 16KB and 4KB page alignment compatibility.
6. **Android Permissions:**
   - Zero additional permissions requested in `AndroidManifest.xml`.
   - The Stage 1 prototype performs purely in-memory computational benchmarks (Vincenty and Haversine distance on WGS-84 ellipsoid). Zero runtime permissions (no GPS, Camera, or Network) are required. Follows least-privilege security principle.
7. **Smoke-Test Isolation:**
   - `prototype_qt/src/app/main.cpp` gates automated exit behind `app.arguments().contains(QStringLiteral("--smoke-test"))`.
   - When launched from Android launcher activity (`android.intent.action.MAIN`), no command-line arguments are passed; `isSmokeTest` evaluates to `false`. The application enters standard event loop `app.exec()` without auto-terminating.
8. **Cross-Platform CMake vs Host Linker Flags:**
   - `prototype_qt/cmake/MinGWConfig.cmake` guards `-fuse-ld=bfd` inside `if(MINGW)`.
   - When cross-compiling for Android, `CMAKE_SYSTEM_NAME` is `Android` and `MINGW` evaluates to `FALSE`. MinGW host linker flags will not pollute the NDK Clang cross-compilation pipeline.
9. **Identified Issue — CTest Target on Android:**
   - In `prototype_qt/CMakeLists.txt`, `enable_testing()` and `add_executable(tst_geodesic tests/tst_geodesic.cpp ...)` are at global scope.
   - When cross-compiling for Android, CMake attempts to compile `tst_geodesic` as an Android ELF binary. CTest cannot run an Android ELF executable directly on a Windows host.
   - *Recommendation for Stage 1C:* Guard unit test executables with `if(NOT ANDROID)` or `if(BUILD_TESTING AND NOT ANDROID)`.

---

## Section E: Missing Components (No Unverified Assumptions)

1. **Qt 6.11.1 Android ARM64 Target Package (`qt.qt6.6111.android`):**
   - *Evidence:* Directory `C:\Qt\6.11.1\android_arm64_v8a` does not exist. `C:\Qt\components.xml` contains only `qt.qt6.6111.win64_mingw`.
   - *Query Result:* `C:\Qt\MaintenanceTool.exe search "qt.qt6.6111.android"` verified that package `qt.qt6.6111.android` version `6.11.1-0-202605091827` is available in the official Qt repository.
2. **JDK 21 Runtime:**
   - *Evidence:* `java -version` returns Oracle JDK `26.0.2`. Android Studio JBR returns `25.0.3`. No JDK 21 installation exists on the system. `JAVA_HOME` is unset.
3. **Android NDK r27c (`27.2.12479018`):**
   - *Evidence:* `C:\Users\sanid\AppData\Local\Android\Sdk\ndk` contains only `27.0.12077973` (r27 initial release). Qt 6.11 reference build requires r27c.
4. **Android Command-line Tools (`cmdline-tools;latest` / `sdkmanager`):**
   - *Evidence:* `C:\Users\sanid\AppData\Local\Android\Sdk\cmdline-tools` is missing. Searching filesystem confirmed `sdkmanager.bat` is not present.
5. **Physical ARM64 Test Device or ARM64 AVD:**
   - *Evidence:* `adb devices -l` returned 0 attached devices. Existing emulator `Pixel_6` is configured for x86_64 architecture.

---

## Section F: Minimum Required Installation Plan

| # | Component | Exact Package / Name | Official Source / Method | Estimated Download Size | Estimated Installed Size | Required for 1st ARM64 APK |
| :- | :--- | :--- | :--- | :--- | :--- | :--- |
| **1** | Qt Android Target | `qt.qt6.6111.android` (contains `android_arm64_v8a`) | Qt Maintenance Tool (`C:\Qt\MaintenanceTool.exe`) | ~520 MB *(estimate)* | ~1.9 GB *(estimate)* | **YES (Mandatory)** |
| **2** | JDK 21 | Eclipse Temurin JDK 21 (x64 Windows) | Adoptium (`adoptium.net`) / MSI or ZIP | ~185 MB *(estimate)* | ~390 MB *(estimate)* | **YES (Mandatory)** |
| **3** | Android NDK r27c | NDK `27.2.12479018` (`r27c`) | Android SDK Manager / developer.android.com | ~670 MB *(estimate)* | ~3.1 GB *(estimate)* | **YES (Mandatory)** |
| **4** | Android CLI Tools | `cmdline-tools;latest` (contains `sdkmanager.bat`) | Android Studio SDK Manager or Google archive | ~135 MB *(estimate)* | ~210 MB *(estimate)* | **Recommended** |
| **5** | ARM64 Device/AVD | Physical ARM64 device or `arm64-v8a` system image | USB Debugging or SDK System Image | ~1.1 GB *(if AVD)* | ~3.5 GB *(if AVD)* | **Required for Device Test** |

---

## Section G: Proposed Environment Variables (Project-Local Session Scope)

To prevent modifying global Windows system settings or conflicting with host tools, the Android toolchain will be initialized strictly within the active shell session using project-local variables:

```powershell
# Project-local session environment (e.g. prototype_qt/env_android.ps1)
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-21.0.x"
$env:ANDROID_SDK_ROOT = "C:\Users\sanid\AppData\Local\Android\Sdk"
$env:ANDROID_HOME = "C:\Users\sanid\AppData\Local\Android\Sdk"
$env:ANDROID_NDK_ROOT = "C:\Users\sanid\AppData\Local\Android\Sdk\ndk\27.2.12479018"

# Isolate session PATH: Prepend local JDK, CMake, Ninja, and ADB (exclude conflicting MinGW32)
$env:Path = "$env:JAVA_HOME\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools;" + $env:Path
```

---

## Section H: Proposed First Android ARM64 Build Command

Once the required components are approved and installed, the first ARM64 build will be configured using the official Qt 6.11 command-line workflow:

### 1. CMake Project Configuration (ARM64 Target)
```powershell
& "C:\Qt\6.11.1\android_arm64_v8a\bin\qt-cmake.bat" `
    -DQT_ANDROID_ABIS="arm64-v8a" `
    -DANDROID_SDK_ROOT="C:\Users\sanid\AppData\Local\Android\Sdk" `
    -DANDROID_NDK_ROOT="C:\Users\sanid\AppData\Local\Android\Sdk\ndk\27.2.12479018" `
    -S "C:\Users\sanid\Documents\GitHub\GeoRescuX\prototype_qt" `
    -B "C:\Users\sanid\Documents\GitHub\GeoRescuX\prototype_qt\build_android_arm64" `
    -G Ninja
```

### 2. Compilation and APK Packaging
```powershell
& "C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "C:\Users\sanid\Documents\GitHub\GeoRescuX\prototype_qt\build_android_arm64" --target apk
```

**Expected Resulting Package:**  
`prototype_qt/build_android_arm64/android-build/appGeoRescuXQtPrototype.apk`

---

## Section I: Planned Device Validation Procedure

1. **Verify Target Device Connection:**
   - Connect physical Android device via USB.
   - Verify device is detected and authorized:
     ```powershell
     & "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools\adb.exe" devices -l
     ```
   - Query device CPU ABI to verify ARM64 capability:
     ```powershell
     & "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools\adb.exe" shell getprop ro.product.cpu.abi
     # Expected output: arm64-v8a
     ```
2. **Install Sandbox Package:**
   ```powershell
   & "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools\adb.exe" install -r "C:\Users\sanid\Documents\GitHub\GeoRescuX\prototype_qt\build_android_arm64\android-build\appGeoRescuXQtPrototype.apk"
   ```
3. **Launch Application Activity:**
   ```powershell
   & "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools\adb.exe" shell am start -n com.example.georescux.qtprototype/org.qtproject.qt.android.bindings.QtActivity
   ```
4. **Logcat Runtime Verification:**
   - Filter Logcat for GeoRescuX Qt logs:
     ```powershell
     & "C:\Users\sanid\AppData\Local\Android\Sdk\platform-tools\adb.exe" logcat -s "GeoRescuX" "Qt"
     ```
   - Verify startup logs:
     - `[GeoRescuX] Starting Stage 1 Qt Prototype runtime. C++20 standard active.`
     - `[GeoRescuX] Root QML scene loaded successfully.`
     - `[Lifecycle] Application state transitioned to: Active (Foreground)`
5. **Interactive UI Verification:**
   - Tap `"Execute C++ Geodesic Benchmark"` button on the device screen.
   - Confirm Vincenty distance calculation displays: `34082.4 m` (Dehradun to Rishikesh).
6. **Coexistence Check:**
   - Verify production GeoRescuX (`com.example.georescux`) remains installed, uncorrupted, and runnable on the same device.

---

## Section J: Risks, Blockers, and Unresolved Questions

1. **Blocker 1 (Missing Qt Android Package):** Without `qt.qt6.6111.android` installed, `qt-cmake.bat` for Android is not present, making cross-compilation impossible.
2. **Blocker 2 (Host JDK Incompatibility):** Host JDK 26 and JBR 25 will trigger Gradle daemon errors (`Unsupported class file major version`) during `androiddeployqt` Gradle invocation. JDK 21 must be installed.
3. **Blocker 3 (Missing ARM64 Hardware / Emulator):** No physical ARM64 test device is currently plugged in, and existing emulator `Pixel_6` is x86_64.
4. **Risk 1 (NDK Version Deviation):** Current installed NDK is r27 (`27.0.12077973`), whereas Qt 6.11 was validated against r27c (`27.2.12479018`). While basic C++20 might compile, potential symbol discrepancies with Qt's prebuilt C++ runtime could occur.
5. **Risk 2 (Host PATH Pollution):** During audit, system `PATH` contained 32-bit `C:\MinGW\bin`, causing DLL load failures (`0xc000007b`) on host binaries unless Qt MinGW64 was prepended. All Android scripts must maintain clean, isolated session PATHs.
6. **CMake Target Warning:** `tst_geodesic` in `CMakeLists.txt` should be guarded with `if(NOT ANDROID)` prior to building to prevent CMake from generating host test targets for Android ELF binaries.

---

## Audit Stopping Condition Compliance

- Desktop build and automated smoke test re-verified: **100% Passing**.
- Production paths verified untouched via Git: **Clean**.
- No Android toolchain components were downloaded or installed.
- No speculative changes were made.
- Execution halted per Stage 1B stopping conditions.
