# GeoRescuX — Complete Repository Structure & Module Guide

## 1. Directory Tree & Functional Subsystems

```
GeoRescuX/
├── app/                                       # Primary Kotlin Android Application (Production)
│   ├── build.gradle.kts                       # AGP 8.8.0, Kotlin 2.0.21 build script
│   ├── src/main/
│   │   ├── AndroidManifest.xml                # Permissions, services, activities, receivers
│   │   ├── assets/                            # Bundled offline map packages & route graphs
│   │   │   ├── maps/                          # MapsForge binary vectors (.map) & GeoJSON places
│   │   │   └── route_graph_uttarakhand        # Binary preprocessed OSM routing graph (~18 MB)
│   │   ├── cpp/                               # C++20 Shared Native Performance Engine (libgeorescux_routing.so)
│   │   │   ├── CMakeLists.txt                 # NDK build configuration
│   │   │   ├── analytics/                     # Casualty survival probability modeling
│   │   │   ├── audio/                         # Acoustic resonance detection for trapped survivors
│   │   │   ├── crypto/                        # HMAC-SHA256 packet signature validation
│   │   │   ├── jni/                           # JNI glue code binding Kotlin to C++
│   │   │   ├── mesh/                          # Packet framing, Bloom filter deduplication, epidemic routing
│   │   │   ├── network/                       # Forward error correction & adaptive bandwidth compression
│   │   │   ├── routing/                       # A*, Contraction Hierarchies, turn penalties, safe havens
│   │   │   ├── spatial/                       # Geodesic ellipsoidal math, KD-Tree, DEM models, hazard polygons
│   │   │   ├── telemetry/                     # Extended Kalman Filter sensor fusion & dead reckoning
│   │   │   └── triage/                        # START disaster triage classification trees
│   │   ├── kotlin/com/example/georescux/      # Kotlin Production Source Code
│   │   │   ├── GeoRescuXApplication.kt        # Process application class & AppContainer manual DI container
│   │   │   ├── MainActivity.kt                # Session trampoline & authentication router
│   │   │   ├── core/                          # JNI native bridge loader, Result types, input validators
│   │   │   ├── data/                          # Data sources, SQLite/Prefs stores, repositories, BLE, sync
│   │   │   ├── domain/                        # Domain entities, use cases, business rules, repository contracts
│   │   │   ├── ui/                            # Activities, view models, bottom sheets, navigation adapters
│   │   │   └── work/                          # WorkManager background sync workers & schedulers
│   │   └── res/                               # Android XML layouts, drawables, styles, strings, themes
│   └── src/test/kotlin/                       # JVM Unit Tests (Robolectric / JUnit / Coroutines-test)
├── prototype_qt/                              # Native C++20 / Qt 6.11.1 Cross-Platform Prototype
│   ├── CMakeLists.txt                         # Multiplatform build (MinGW 64-bit Desktop & Android ARM64)
│   ├── src/
│   │   ├── app/                               # AppState authoritative state machine & main entry point
│   │   ├── domain/                            # Services, models, and interface contracts (IEmergencyRepository, etc.)
│   │   └── platform/                          # SQLite repositories, Qt location provider, QQuickPaintedItem map view
│   ├── ui/qml/                                # QML user interface views (CitizenHome, SafeRoute, Map, Lab)
│   └── tests/                                 # CTest automated test suites (emergency, routing, location, map)
├── backend/                                   # High-Performance C++ Drogon Web Microservice
│   ├── CMakeLists.txt                         # Drogon & SQLite3 build script
│   ├── main.cc                                # Server entry point (port 8080)
│   ├── controllers/                           # REST endpoints (SOS, BLE mesh, disaster clusters, health)
│   ├── dispatch/                              # Rescue resource matching & Kuhn-Munkres dispatch optimization
│   ├── filters/                               # Firebase JWT authentication filter
│   ├── mesh/                                  # Mesh packet ingestion pipeline
│   ├── models/                                # Data transfer objects (DTOs)
│   ├── realtime/                              # WebSocket command & telemetry broker
│   ├── spatial/                               # Disaster physics engines (wildfire, flood, landslide, plume, seismic)
│   ├── storage/                               # SQLite persistent storage engine
│   └── test/                                  # Drogon unit and simulation tests
├── functions/                                 # Firebase Cloud Functions (Node.js)
│   ├── index.js                               # Callable cloud functions for admin claims
│   └── package.json                           # Functions dependencies
├── georescux-admin-provision/                 # Out-of-Band Admin Provisioning Tooling
│   ├── makeAdmin.js                           # Node script to grant admin custom claims
│   ├── removeAdmin.js                         # Node script to revoke admin custom claims
│   └── manageAdmin.js                         # Interactive admin management CLI
├── docs/                                      # Multi-Tier Engineering Documentation
├── output/ & source/                          # Regional Bundled Map Datasets & Boundaries
├── tools/                                     # Developer Tooling & Map Preprocessors
├── database.rules.json                        # Firebase Realtime Database Security Rules
├── firebase.json                              # Firebase Project Deployment Configuration
├── build.gradle.kts                           # Root Gradle Project Build Script
└── settings.gradle.kts                        # Gradle Module Inclusions
```

---

## 2. Folder-by-Folder Responsibility Matrix

### `app/` — Kotlin Android Production Application
- **Purpose**: Main production mobile client for Android devices (API 24 to 35).
- **Key Modules**:
  - `src/main/kotlin/.../ui/`: Material Design activities, fragments, and bottom sheets for SOS, routing, contacts, and admin management.
  - `src/main/kotlin/.../data/`: Implementations of repositories, local SQLite and SharedPreferences stores, BLE managers, and Firebase sync connectors.
  - `src/main/kotlin/.../domain/`: Pure business logic, use cases (`StartSosUseCase`, etc.), and interface contracts.
  - `src/main/cpp/`: C++20 algorithms compiled via CMake into `libgeorescux_routing.so`.
  - `src/main/assets/`: Pre-bundled regional MapsForge vector maps and graph topologies.
- **Dependencies**: Android SDK 35, Jetpack, OSMDroid 6.1, MapsForge, Google Nearby, Firebase BOM 34.5.

### `prototype_qt/` — Native C++20 / Qt 6.11.1 Prototype
- **Purpose**: High-performance, cross-platform citizen client prototype designed for rapid tactical deployment on desktop (Windows/Linux) and Android.
- **Key Modules**:
  - `src/app/`: `AppState` state machine and application entry point.
  - `src/domain/`: Domain models (`EmergencyModel`, `LocationModel`) and interfaces (`IEmergencyRepository`).
  - `src/platform/`: SQLite database implementations and Qt Quick painted map components.
  - `ui/qml/`: High-framerate QML UI.
  - `tests/`: CTest test suites executed with MinGW.
- **Dependencies**: Qt 6.11.1 (Core, Gui, Quick, Sql), MinGW 13.1.0 / Android Clang.

### `backend/` — C++ Drogon Web Service
- **Purpose**: High-throughput incident processing, spatial hazard clustering, and dispatch optimization server for emergency operation centers.
- **Key Modules**:
  - `controllers/`: REST endpoints for SOS ingestion, BLE mesh packet collection, and disaster clusters.
  - `spatial/`: Physical hazard simulation models (Wildfire Rothermel, Hydrodynamic Flood, Avalanche Snowpack, Landslide Susceptibility).
  - `dispatch/`: Rescue resource matching and Kuhn-Munkres dispatch optimization.
  - `storage/`: SQLite persistent storage engine.
- **Dependencies**: Drogon C++ Framework, SQLite3, C++20.

### `functions/` — Firebase Cloud Functions
- **Purpose**: Serverless Node.js backend for inspecting and assigning administrative claims.
- **Dependencies**: Firebase Admin SDK, Firebase Functions SDK.

### `georescux-admin-provision/` — Admin Provisioning Tooling
- **Purpose**: Out-of-band administrative script to securely grant or revoke the `admin: true` custom claim on Firebase accounts without exposing cloud function vulnerabilities.
- **Dependencies**: Node.js, `serviceAccountKey.json`.

### `output/` & `source/` — Regional Datasets
- **Purpose**: Partitioned regional boundary GeoJSON, manifests, and OpenStreetMap PBF extracts covering Uttarakhand, Himachal Pradesh, Haryana, and Uttar Pradesh.
