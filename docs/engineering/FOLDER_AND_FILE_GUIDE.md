# GeoRescuX — Complete Folder & File-by-File Guide

## 1. Directory Tree & Subsystem Taxonomy

```
GeoRescuX/
├── app/                                       # Production Android Application
│   ├── build.gradle.kts                       # AGP & dependency build script
│   ├── src/main/
│   │   ├── AndroidManifest.xml                # Permissions, services, activities, receivers
│   │   ├── assets/                            # Bundled offline map packages & route graphs
│   │   │   ├── maps/                          # MapsForge binary vectors (.map) & GeoJSON places
│   │   │   └── route_graph_uttarakhand        # Binary preprocessed OSM routing graph (~18 MB)
│   │   ├── cpp/                               # C++20 Shared Native Performance Engine
│   │   │   ├── CMakeLists.txt                 # NDK build configuration for libgeorescux_routing.so
│   │   │   ├── analytics/                     # Casualty survival probability algorithms
│   │   │   ├── audio/                         # Acoustic resonance detection for trapped victims
│   │   │   ├── crypto/                        # HMAC-SHA256 packet authentication
│   │   │   ├── jni/                           # JNI bindings for Kotlin-C++ boundary
│   │   │   ├── mesh/                          # Packet framing, bloom filter deduplication, epidemic routing
│   │   │   ├── network/                       # Forward error correction & adaptive compression
│   │   │   ├── routing/                       # A*, Contraction Hierarchies, turn penalties, safe haven fallback
│   │   │   ├── spatial/                       # Geodesic math, KD-Tree, DEM elevation models, hazard polygons
│   │   │   ├── telemetry/                     # Kalman filter sensor fusion & dead reckoning
│   │   │   └── triage/                        # START medical triage decision trees
│   │   ├── kotlin/com/example/georescux/      # Kotlin Production Source Tree
│   │   │   ├── GeoRescuXApplication.kt        # Application lifecycle & AppContainer DI container
│   │   │   ├── MainActivity.kt                # Session router (Login vs Home vs Region Selection)
│   │   │   ├── core/                          # Native bridge loaders, Result types, validators
│   │   │   ├── data/                          # Data sources, SQLite/Prefs stores, repositories, BLE, sync
│   │   │   ├── domain/                        # Domain models, use cases, business rules, repository contracts
│   │   │   ├── ui/                            # Activities, view models, fragments, bottom sheets, adapters
│   │   │   └── work/                          # WorkManager background sync workers & schedulers
│   │   └── res/                               # Android XML layouts, drawables, styles, strings, themes
│   └── src/test/kotlin/                       # JVM Unit Tests (Robolectric / JUnit / Coroutines-test)
├── prototype_qt/                              # Native C++20 / Qt 6.11.1 Android & Desktop Prototype
│   ├── CMakeLists.txt                         # CMake multiplatform build (MinGW 64-bit / Android ARM64)
│   ├── src/
│   │   ├── app/                               # AppState authoritative state machine & main entry point
│   │   ├── domain/                            # Services, models, and interface contracts (IEmergencyRepository, etc.)
│   │   └── platform/                          # SQLite repositories, Qt location provider, QQuickPaintedItem map view
│   ├── ui/qml/                                # QML user interface views (CitizenHome, SafeRoute, Map, Lab)
│   └── tests/                                 # CTest automated test suites (emergency, routing, location, map)
├── backend/                                   # High-Performance C++ Drogon Web Service
│   ├── CMakeLists.txt                         # Drogon & SQLite3 build script
│   ├── main.cc                                # Server entry point (port 8080)
│   ├── controllers/                           # REST endpoints (SOS, BLE mesh, disaster clusters, health)
│   ├── dispatch/                              # Rescue resource matching & dispatch optimization
│   ├── filters/                               # Firebase JWT authentication filter
│   ├── mesh/                                  # Mesh packet ingestion pipeline
│   ├── models/                                # Data transfer objects (DTOs)
│   ├── realtime/                              # WebSocket command & telemetry broker
│   ├── spatial/                               # Physical hazard engines (wildfire, flood, landslide, plume, seismic)
│   ├── storage/                               # SQLite persistent storage engine
│   └── test/                                  # Drogon unit and simulation tests
├── functions/                                 # Firebase Cloud Functions (Node.js)
│   ├── index.js                               # Callable cloud functions for admin claims
│   └── package.json                           # Functions dependencies
├── georescux-admin-provision/                 # Out-of-Band Admin Provisioning Tooling
│   ├── makeAdmin.js                           # Node script to grant admin custom claims
│   ├── removeAdmin.js                         # Node script to revoke admin custom claims
│   └── manageAdmin.js                         # Interactive admin management CLI
├── output/ & source/                          # Regional Map Packages & Boundaries
│   ├── haryana/                               # Haryana manifest & GeoJSON boundary
│   ├── himachal_pradesh/                      # Himachal Pradesh manifest & GeoJSON boundary
│   ├── uttar_pradesh/                         # Uttar Pradesh manifest & GeoJSON boundary
│   └── uttarakhand/                           # Uttarakhand manifest, boundary, and OSM PBF data
├── tools/                                     # Developer Tooling & Build Scripts
│   ├── convert_to_map.ps1                     # PowerShell wrapper for MapsForge map-writer
│   └── cpp_graph_builder/                     # Standalone OSM PBF to JSON graph compiler
├── database.rules.json                        # Firebase Realtime Database security rules
├── firebase.json                              # Firebase deployment configuration
├── build.gradle.kts                           # Root Gradle build configuration
└── settings.gradle.kts                        # Gradle module declarations & plugin management
```

---

## 2. File-by-File Inventory & Responsibility Matrix

### A. Root Configuration & Security Files

| File Path | Format | Responsibility | Main Components | Dependencies / Callers | Status | Verified Notes / Actions |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `build.gradle.kts` | Kotlin DSL | Root Gradle project build script. | Plugin management, cleanup tasks. | Gradle Toolchain | VERIFIED WORKING | Configured for AGP 8.8.0 and Kotlin 2.0.21. |
| `settings.gradle.kts` | Kotlin DSL | Gradle settings & repository declarations. | `include(":app")`, Maven Central, Google. | Gradle Daemon | VERIFIED WORKING | Cleanly sets up dependency resolution. |
| `gradle.properties` | Properties | Build configuration parameters. | `org.gradle.jvmargs`, AndroidX flags. | Gradle Build | VERIFIED WORKING | Configures JVM heap and AndroidX migrations. |
| `database.rules.json` | JSON | Firebase Realtime Database Security Rules. | Path authorization rules for `/sos_alerts`, `/contacts`, `/hazards`, `/incidents`. | Firebase Realtime DB | VERIFIED WORKING | Enforces `auth.token.admin == true` for admin paths. |
| `firebase.json` | JSON | Firebase CLI configuration. | Database rules and functions deploy mapping. | Firebase CLI | VERIFIED WORKING | Points database rules to `database.rules.json`. |

---

### B. Core Android Application (`app/src/main/kotlin/com/example/georescux/`)

#### 1. Lifecycle, Application Container & Dispatch
- **`GeoRescuXApplication.kt`**:
  - *Responsibility*: Process-level application class and manual dependency injection container (`AppContainer`).
  - *Main Classes*: `GeoRescuXApplication : Application`, `AppContainer(context: Context)`.
  - *Dependencies*: Instantiates singletons: `AuthRepositoryImpl`, `SosRepositoryImpl`, `SyncingSosRepository`, `SosRelayRepository`, `SosBleBridgeRepository`, `RouteRepositoryImpl`, `SyncRetryCoordinator`, `GeoRescueBleManager`.
  - *Status*: `VERIFIED WORKING`. Background initialization prevents ANR on cold starts.
- **`MainActivity.kt`**:
  - *Responsibility*: Entry-point trampoline activity that evaluates authentication state and routes users to either `LoginActivity`, `RegionSelectionActivity`, or `HomeActivity`.
  - *Main Classes*: `MainActivity : AppCompatActivity`.
  - *Dependencies*: Uses `authRepository.currentUser` and `appContainer.hasSelectedRegion`.
  - *Status*: `VERIFIED WORKING`. Ensures smooth navigation flow.

#### 2. Authentication Subsystem (`data/auth/`, `domain/auth/`, `ui/auth/`)
- **`data/auth/AuthRepositoryImpl.kt`**:
  - *Responsibility*: Wraps Firebase Authentication to provide login, registration, password reset, and session observation.
  - *Main Classes*: `AuthRepositoryImpl : AuthRepository`.
  - *Status*: `VERIFIED WORKING`. Safely handles Firebase network timeouts.
- **`data/auth/FirebaseRoleResolver.kt`**:
  - *Responsibility*: Inspects Firebase Auth custom claims (`claims["admin"] == true`) with token refresh capabilities.
  - *Main Classes*: `FirebaseRoleResolver : RoleResolver`.
  - *Status*: `VERIFIED WORKING`. Fails closed to `UserRole.UNKNOWN` when network is absent and token is expired.
- **`ui/auth/LoginActivity.kt` & `RegisterActivity.kt`**:
  - *Responsibility*: User-facing authentication screens supporting email/password and Google Sign-in.
  - *Status*: `VERIFIED WORKING`. Contrast defects resolved; active tabs display white text on primary blue.

#### 3. Emergency SOS Subsystem (`data/sos/`, `domain/sos/`, `ui/sos/`)
- **`domain/sos/SosEmergency.kt`**:
  - *Responsibility*: Immutable domain entity representing a life-safety distress event.
  - *Main Properties*: `id`, `startedAtMs`, `stoppedAtMs`, `location: SosLocation?`, `locationStatus`, `note`.
  - *Status*: `VERIFIED WORKING`.
- **`data/sos/SosEmergencyJson.kt`**:
  - *Responsibility*: Safe JSON serializer and deserializer for persisting SOS records to SharedPreferences.
  - *Status*: `VERIFIED WORKING`. Note handling sanitized against literal `"null"` string serialization defect.
- **`data/sos/SosRepositoryImpl.kt`**:
  - *Responsibility*: Local-first SOS storage implementation maintaining active emergency state and historical logs.
  - *Main Classes*: `SosRepositoryImpl : SosRepository`.
  - *Status*: `VERIFIED WORKING`. Protected by internal thread synchronization locks.
- **`data/sos/SyncingSosRepository.kt`**:
  - *Responsibility*: Decorator repository that mirrors local emergency state to Firebase Realtime Database at `/sos_alerts/{uid}`.
  - *Status*: `VERIFIED WORKING`. Coordinates with `SyncStateStore` for durable offline queuing.
- **`ui/sos/SosActivity.kt` & `SosViewModel.kt`**:
  - *Responsibility*: 5-second countdown trigger, active emergency monitoring, live coordinate display, and emergency termination.
  - *Status*: `VERIFIED WORKING`. High-contrast UI, full-width GPS display, and prominent red "END EMERGENCY" action button.

#### 4. BLE Mesh Subsystem (`data/ble/`, `domain/ble/`, `ui/ble/`)
- **`data/ble/GeoRescueBleManager.kt`**:
  - *Responsibility*: Core controller for BLE advertising and scanning using standard GATT services (UUID `0x180D`).
  - *Main Features*: Dynamic MTU negotiation, packet acceptance listener, scan/advertise lifecycle management.
  - *Status*: `IMPLEMENTED — NOT PHYSICALLY VERIFIED`. Fully unit-tested; requires dual physical hardware for RF validation.
- **`data/ble/GeoRescueBlePacketCodec.kt`**:
  - *Responsibility*: Compact binary serializer packing emergency payloads (GPS coordinates, timestamp, battery level, hop count, sender ID) into BLE advertisement frames.
  - *Status*: `VERIFIED WORKING`. Validated against boundary and truncation test cases.
- **`data/relay/GoogleNearbyClientAdapter.kt` & `RelayEngine.kt`**:
  - *Responsibility*: Secondary mesh transport utilizing Google Nearby Connections for multi-hop epidemic event forwarding.
  - *Status*: `VERIFIED WORKING`. Integrated with `SQLiteSeenEventStore` for Bloom-filter deduplication.

#### 5. Offline Routing & Safe Route Pro (`data/routing/`, `domain/routing/`, `ui/routing/`)
- **`domain/routing/AStarPathfinder.kt`**:
  - *Responsibility*: Pure Kotlin A* pathfinding algorithm over `RouteGraph`. Admissible Haversine distance heuristic.
  - *Status*: `VERIFIED WORKING`. Integrates dynamic hazard penalties and blocked-edge exclusion.
- **`domain/routing/SafeHavenFallback.kt`**:
  - *Responsibility*: Fallback mechanism when the citizen's requested destination is unreachable due to catastrophe; redirects to nearest accessible safe haven.
  - *Status*: `VERIFIED WORKING`.
- **`data/routing/GeoRescueNativeRouting.kt`**:
  - *Responsibility*: JNI bridge to `libgeorescux_routing.so` with automatic, safe fallback to Kotlin `SafeHavenFallback` if native library is unavailable.
  - *Status*: `VERIFIED WORKING`.
- **`ui/routing/RouteActivity.kt` & `RouteViewModel.kt`**:
  - *Responsibility*: Evacuation route planning UI, destination selection, turn-by-turn guidance, and MapsForge vector rendering.
  - *Status*: `VERIFIED WORKING`. DPI tile scaling enabled (`isTilesScaledToDpi = true`) with honest online/offline source badges.

#### 6. Administration & Regional Command (`data/admin/`, `ui/admin/`)
- **`ui/admin/AdminDashboardActivity.kt`**:
  - *Responsibility*: Central command console for incident triage, SOS administration, road blocking, hazard creation, and public alerts.
  - *Status*: `VERIFIED WORKING`. Card hierarchy visual clash fixed, bottom navigation padding (84dp) applied, and note displays sanitized.
- **`data/alerts/AdminAlertObserver.kt`**:
  - *Responsibility*: Listens to regional `/admin_alerts` and `/hazards` on Firebase Realtime Database and notifies the user.
  - *Status*: `VERIFIED WORKING`.

---

### C. C++ Shared Native Engine (`app/src/main/cpp/`)

| File Path | Responsibility | Key Classes / Functions | Tested By | Status |
| :--- | :--- | :--- | :--- | :--- |
| `routing/RouteGraph.cpp` | In-memory road topology graph. | `RouteGraph`, `Node`, `Edge`. | `tst_routing.cpp` | VERIFIED WORKING |
| `routing/AStarPathfinder.cpp` | C++20 A* search with hazard weighting. | `AStarPathfinder::findRoute`. | `tst_routing.cpp` | VERIFIED WORKING |
| `spatial/GeodesicMath.cpp` | Haversine & Vincenty ellipsoidal distance. | `haversineMeters`, `vincentyDistanceMeters`. | `tst_geodesic.cpp` | VERIFIED WORKING |
| `mesh/BloomFilterDeduplicator.cpp` | Fast packet duplicate suppression. | `BloomFilterDeduplicator`. | `tst_foundation.cpp` | VERIFIED WORKING |
| `mesh/MeshPacketCodec.cpp` | Binary bit-packing of BLE mesh packets. | `encodePacket`, `decodePacket`. | `tst_foundation.cpp` | VERIFIED WORKING |
| `crypto/PacketAuthenticator.cpp` | HMAC-SHA256 integrity verification. | `computeHmacSha256`, `verifyPacket`. | `tst_foundation.cpp` | VERIFIED WORKING |
| `triage/TriageEngine.cpp` | START medical casualty classification. | `classifyCasualty(canWalk, pulse, etc.)`. | `tst_foundation.cpp` | VERIFIED WORKING |

---

### D. Native Qt Prototype (`prototype_qt/`)

| File Path | Responsibility | Key Classes / Functions | Tested By | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/app/AppState.cpp` | Unified state machine and QML context router. | `AppState : public QObject`. | `tst_foundation.cpp` | VERIFIED WORKING |
| `src/domain/EmergencyService.cpp` | Emergency countdown and activation service. | `EmergencyService : public QObject`. | `tst_emergency.cpp` | VERIFIED WORKING |
| `src/platform/storage/SqliteEmergencyRepository.cpp` | SQLite persistent SOS repository. | `SqliteEmergencyRepository : public IEmergencyRepository`. | `tst_emergency.cpp` | VERIFIED WORKING |
| `src/platform/storage/LocalDatabase.cpp` | SQLite connection manager & schema migrations. | `LocalDatabase`. | `tst_foundation.cpp` | VERIFIED WORKING |
| `src/domain/MapPackageManager.cpp` | Regional manifest and boundary parser. | `MapPackageManager : public QObject`. | `tst_map_package.cpp` | VERIFIED WORKING |
| `src/domain/RouteService.cpp` | QML-facing evacuation routing service. | `RouteService : public QObject`. | `tst_routing.cpp` | VERIFIED WORKING |
| `src/platform/qt/QtLocationProvider.cpp` | QGeoPositionInfoSource hardware location wrapper. | `QtLocationProvider : public ILocationProvider`. | `tst_location.cpp` | VERIFIED WORKING |
| `ui/qml/Main.qml` | Top-level QML application container. | Citizen navigation stack, top bar, modals. | Desktop Smoke Test | VERIFIED WORKING |

---

### E. Backend Microservice (`backend/`)

| File Path | Responsibility | Key Classes / Functions | Tested By | Status |
| :--- | :--- | :--- | :--- | :--- |
| `main.cc` | Drogon C++ server startup and configuration. | `main()`, `drogon::app().run()`. | Manual launch | VERIFIED WORKING |
| `storage/SqliteStorage.cc` | High-throughput SQLite event persistence. | `SqliteStorage::insertSosEvent`. | `backend_test.cc` | VERIFIED WORKING |
| `controllers/SosEventController.cc` | REST endpoint for receiving SOS telemetry. | `SosEventController::postSosEvent`. | `backend_test.cc` | VERIFIED WORKING |
| `controllers/BleMeshController.cc` | Ingestion endpoint for relayed BLE mesh packets. | `BleMeshController::postMeshPacket`. | `backend_test.cc` | VERIFIED WORKING |
| `spatial/DisasterSpreadSimulator.cc` | Spatial physics simulation orchestrator. | `DisasterSpreadSimulator::simulateSpread`. | `disaster_simulation_test.cc` | VERIFIED WORKING |
| `dispatch/RescueDispatchOptimizer.cc` | Kuhn-Munkres resource-casualty assignment. | `RescueDispatchOptimizer::optimizeDispatch`. | `disaster_simulation_test.cc` | VERIFIED WORKING |
