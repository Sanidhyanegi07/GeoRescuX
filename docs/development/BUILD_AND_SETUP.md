# GeoRescuX — Developer Setup, Build & Toolchain Guide

## 1. System Requirements & Prerequisites

To develop, build, and test GeoRescuX, your workstation should meet the following specifications:
- **Operating System**: Windows 10/11 64-bit, Ubuntu 22.04+ LTS, or macOS 13+
- **JDK**: Java Development Kit 17 (Eclipse Temurin or OpenJDK)
- **Android SDK**: API Level 35 (Android 15) with Build Tools `35.0.0`
- **Android NDK**: Version `25.1.8937393` or higher
- **CMake**: Version `3.22.1` or higher
- **Qt Framework**: Qt `6.11.1` (or 6.8+ LTS) with `Core`, `Gui`, `Quick`, `Sql`, and `Location` modules
- **C++ Compiler**: MinGW GCC `13.1.0` 64-bit (for Desktop) and Clang (for Android NDK)
- **Node.js**: v18+ with `npm` (for admin provisioning)

---

## 2. Kotlin Android Production Build (`app/`)

### A. Environment Configuration
Set your environment variables (PowerShell):
```powershell
$env:ANDROID_HOME = "C:\Users\<username>\AppData\Local\Android\Sdk"
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.x.x"
$env:PATH = "$env:ANDROID_HOME\platform-tools;$env:JAVA_HOME\bin;" + $env:PATH
```

### B. Building the Release APK
From the repository root:
```powershell
.\gradlew.bat assembleRelease
```
- Output: `app\build\outputs\apk\release\app-release-unsigned.apk`
- The build automatically compiles native C++20 algorithms via CMake into `libgeorescux_routing.so` for all 4 Android architectures (`arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`).

### C. Running Automated Unit Tests
```powershell
.\gradlew.bat testDebugUnitTest
```
- Executes all JVM unit tests (graph parsing, A\* routing, safe haven fallback, SOS serialization).

---

## 3. Native Qt C++ Prototype Build (`prototype_qt/`)

### A. Desktop Target (MinGW 64-bit)
1. Configure Environment:
   ```powershell
   $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;" + $env:PATH
   ```
2. Configure with CMake:
   ```powershell
   cmake -B prototype_qt/build_desktop -S prototype_qt -G "Ninja" -DCMAKE_BUILD_TYPE=Release
   ```
3. Compile Target:
   ```powershell
   cmake --build prototype_qt/build_desktop --target appGeoRescuXQtPrototype
   ```
4. Run Desktop Automated CTests:
   ```powershell
   ctest --test-dir prototype_qt/build_desktop --output-on-failure
   ```

### B. Android ARM64 Target (Qt Android APK)
1. Configure with Android Toolchain:
   ```powershell
   & prototype_qt/env_android.ps1
   cmake -B prototype_qt/build_android_arm64 -S prototype_qt -G "Ninja" `
     -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" `
     -DANDROID_ABI="arm64-v8a" `
     -DANDROID_PLATFORM=android-24
   ```
2. Build APK:
   ```powershell
   cmake --build prototype_qt/build_android_arm64 --target appGeoRescuXQtPrototype_make_apk
   ```
- Output: `prototype_qt\build_android_arm64\android-build\appGeoRescuXQtPrototype.apk`

---

## 4. C++ Drogon Microservice Backend Build (`backend/`)

### Linux / WSL / MSYS2 Build
```bash
cd backend
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/georescux_backend
```
- Starts Drogon HTTP REST server on port `8080`.
- Automated test suites:
  ```bash
  ctest --test-dir build --output-on-failure
  ```

---

## 5. Firebase & Administrator Provisioning

### Minting an Admin Claim
1. Place your private service account key at `georescux-admin-provision/serviceAccountKey.json`.
2. Run the provisioning script:
   ```powershell
   cd georescux-admin-provision
   npm install
   node makeAdmin.js <target-firebase-uid>
   ```
3. To revoke administrative privileges:
   ```powershell
   node removeAdmin.js <target-firebase-uid>
   ```

---

## 6. Common Build Troubleshooting

1. **OneDrive Access Denied during Gradle builds**:
   - OneDrive file syncing locks intermediate `.class` and `.dex` files.
   - *Fix*: Set the environment variable `GEORESCUX_BUILD_DIR` to a non-synced drive (e.g. `C:\builds\georescux`). `app/build.gradle.kts` automatically relocates build output when this variable is present.
2. **OutOfMemoryError during `testDebugUnitTest`**:
   - Parsing the bundled 18 MB Uttarakhand graph requires high JVM heap.
   - *Fix*: `app/build.gradle.kts` configures `tasks.withType<Test> { maxHeapSize = "3g" }`.
3. **16 KB Page Alignment on Android 15**:
   - *Fix*: `app/build.gradle.kts` specifies `useLegacyPackaging = true` to ensure JNI native libraries are compressed and aligned.
