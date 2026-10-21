# GeoRescuX — System Architecture Specification

## 1. Architectural Philosophy & Principles

GeoRescuX is engineered on four non-negotiable architectural pillars:
1. **Local-First Autonomy**: The client device must function completely when detached from all telecommunications infrastructure. All business logic, emergency state changes, and routing algorithms execute locally without network calls.
2. **Layered Separation of Concerns**: Clean separation between Presentation (UI/Views), Domain (Pure Business Rules & Use Cases), and Data (Persistence, BLE, Cloud Sync).
3. **Decoupled Cross-Platform Core**: Reusable C++20 algorithms (A\* pathfinding, Vincenty geodesic distance, Bloom filters, START triage) are compiled natively into both the Android production application (via NDK/JNI) and the Qt cross-platform prototype.
4. **Zero-Trust Security & Fail-Closed Authorization**: Privilege checks never trust client-side state flags; administrative authority requires verified cryptographic claims (`auth.token.admin == true`).

---

## 2. System Layer Breakdown

```
+-----------------------------------------------------------------------------------+
|                            Layered Architecture Model                             |
+-----------------------------------------------------------------------------------+

[PRESENTATION LAYER]
  • Android: Jetpack Activities, ViewModels, Material Design 3 Components, OSMDroid
  • Qt Prototype: QML Declarative Views (CitizenHome, SafeRoute, Map), QQuickPaintedItem
                                      |
                                      v
[DOMAIN LAYER]
  • Use Cases: StartSosUseCase, CancelSosUseCase, ReportHazardUseCase, UpdateLocation
  • Entities: SosEmergency, EvacuationRoute, RoadHazard, DistrictMetrics
  • Contracts: SosRepository, RouteRepository, ContactsRepository, RoleResolver
                                      |
                                      v
[DATA & PLATFORM LAYER]
  • Local Stores: SharedPreferencesSosStore, SharedPreferencesRouteGraphStore, SQLite
  • Native JNI Engine: libgeorescux_routing.so (A*, ElevationCost, BloomFilter, Crypto)
  • Mesh Relays: GeoRescueBleManager (GATT 0x180D), GoogleNearbyClientAdapter
  • Cloud Sync: FirebaseSosCloudDataSource, SyncRetryCoordinator, WorkManagerSyncScheduler
```

---

## 3. Concurrency & Threading Model

### Android Kotlin Application
- **Main Thread (`Dispatchers.Main.immediate`)**: Dedicated strictly to UI rendering, view binding, and user interaction. No file I/O or JSON decoding is permitted on this thread.
- **Worker Thread Pool (`Dispatchers.IO` / `Dispatchers.Default`)**:
  - `GeoRescuXApplication.onCreate()` launches container setup asynchronously so `FirebaseAuth` initialization does not block the cold-start activity render.
  - Large regional graph parsing (~18 MB OSM JSON) runs on background dispatchers with synchronized in-memory memoization.
  - Periodic and network-constrained synchronizations execute inside Android Jetpack `WorkManager` background threads (`SyncWorker`).

### Qt C++ Prototype
- **Main GUI Thread**: Owns the Qt Event Loop, QML engine property bindings, and `QQuickPaintedItem` map rendering.
- **Worker Threads & Non-Blocking Signals**:
  - `SqliteEmergencyRepository` executes disk transactions synchronously within short-lived SQLite RAII transactions.
  - Hardware GPS location acquisition emits asynchronous Qt signals (`positionUpdated`) from platform-managed threads, marshaled cleanly into the main thread via Qt's Meta-Object System.

### Drogon C++ Microservice
- **Multi-Threaded Event Loop**: Drogon operates an epoll/kqueue multi-reactor architecture across a thread pool sized to hardware CPU cores (`std::thread::hardware_concurrency()`).
- **Asynchronous Handlers**: REST controllers for SOS ingestion and mesh packets process incoming requests asynchronously, dispatching compute-heavy disaster simulations to dedicated worker task queues.

---

## 4. Multi-Network Topology (Dual-Mesh + Cloud)

```
[Citizen Phone A (No Signal)] 
       | (BLE GATT Broadcast 0x180D)
       v
[Citizen Phone B (No Signal)] 
       | (Google Nearby Epidemic Forwarding)
       v
[Field Responder Phone C (Intermittent Uplink)]
       |
       +---------------------------------------------+
       | (Cellular / Satellite Connection Detected)  |
       v                                             v
[Firebase Realtime Database]              [Drogon C++ Backend Server]
  • /sos_alerts/{uid}                       • Port 8080 REST Ingestion
  • /hazards                                • DBSCAN Spatial Clustering
  • /incidents                              • Disaster Spread Simulation
       |                                             |
       v                                             v
[Admin Command Center Dashboard]          [Automated Rescue Resource Dispatch]
```
