# GeoRescuX — System Architecture & Codebase Overview

## 1. System Vision & Purpose

GeoRescuX is an emergency-response and disaster-evacuation technology ecosystem designed to function reliably in extreme crisis scenarios, including infrastructure failure, power grid collapse, telecommunications blackouts, and physical network destruction.

When natural catastrophes (floods, earthquakes, landslides, cyclones) or civilian emergencies strike, standard commercial applications fail because they depend on persistent internet connectivity, centralized cloud APIs, and stable cellular infrastructure. GeoRescuX adopts a **Local-First, Offline-Always, Multi-Tier Mesh Architecture** where:
1. Every critical citizen operation (SOS activation, offline evacuation routing, emergency contact alerting, hazard reporting) executes immediately on-device without network delays.
2. Emergency telemetry propagates horizontally between nearby devices over Bluetooth Low Energy (BLE) mesh hops (using both standard GATT broadcast protocols and Google Nearby Connections).
3. Whenever an edge device temporarily gains cellular, satellite, or Wi-Fi uplink, it automatically synchronizes queued emergencies, citizen incident reports, and mesh-relayed packets to centralized emergency response infrastructure.
4. Administrative command centers receive real-time incident clusters, verify high-priority civilian distress calls, broadcast urgent advisories, and deploy rescue resources using advanced spatial modeling engines.

---

## 2. Multi-Tier Ecosystem Architecture

The repository contains three primary application tiers, plus cloud infrastructure and data processing tooling:

```
+-----------------------------------------------------------------------------------+
|                               GeoRescuX Ecosystem                                 |
+-----------------------------------------------------------------------------------+
|  1. Production Android App    |  2. Native Qt C++ Prototype  |  3. Drogon C++ Core|
|     (Kotlin + Android NDK)    |     (C++20 / Qt 6.11.1)      |     (Backend API)  |
|     • Material Design UI      |     • QML Fast UI            |     • High-load    |
|     • OSMDroid + MapsForge    |     • Multiplatform Engine   |       ingestion    |
|     • Android BLE + Nearby    |     • Sqlite Durable Store   |     • Physics Sims |
|     • Firebase Realtime Sync  |     • Shared C++ Routing     |     • Dispatch Opt |
+-------------------------------+------------------------------+--------------------+
                                        |
                   +--------------------+--------------------+
                   |                                         |
                   v                                         v
+------------------------------------+   +------------------------------------------+
|  4. Cloud Infrastructure           |   |  5. Tooling & Regional Assets            |
|     • Firebase Realtime Database   |   |     • Python/C++ OSM Graph Preprocessors |
|     • Firebase Authentication      |   |     • Bundled MapsForge Offline Maps     |
|     • Custom Security Rules        |   |     • Multi-State Boundary GeoJSON       |
|     • Admin Provisioning Scripts   |   |     • Contraction Hierarchy Generators   |
+------------------------------------+   +------------------------------------------+
```

### Module Roles and Technology Stack

| Module | Primary Language | Technology Stack | Operational Role |
| :--- | :--- | :--- | :--- |
| **`app/`** | Kotlin / C++ (NDK) | Kotlin 2.0, Android SDK 35, Jetpack, OSMDroid 6.1, MapsForge, Google Nearby, Firebase BOM 34.5 | Production Android application deployed to citizens and field administrators. Handles offline mapping, SOS broadcasts, mesh relay, and cloud sync. |
| **`prototype_qt/`** | C++20 | Qt 6.11.1 (Core, Gui, Quick, Sql), QML, CMake, MinGW / Android Clang | High-performance, cross-platform citizen client prototype designed for rapid tactical deployment on desktop (Windows/Linux) and Android. |
| **`backend/`** | C++20 | Drogon Web Framework, SQLite3, C++ Threads, JSONCpp | High-throughput disaster management backend for disaster simulation (landslide, flood, wildfire, plume, seismic), spatial clustering, and rescue dispatch. |
| **`functions/`** | JavaScript (Node.js) | Firebase Cloud Functions, Firebase Admin SDK | Cloud functions for role-based custom claims administration. |
| **`georescux-admin-provision/`** | JavaScript (Node.js) | Node.js, Firebase Admin SDK | Standalone out-of-band administrative script to securely grant or revoke the `admin: true` custom claim on Firebase accounts. |
| **`output/` & `source/`** | Data (PBF, JSON, GeoJSON) | OpenStreetMap, GeoJSON, OSM PBF | Bundled regional map packages (Uttarakhand, Himachal Pradesh, Haryana, Uttar Pradesh) with graph topologies and administrative boundaries. |
| **`tools/`** | PowerShell, C++ | OSMOSIS, MapsForge map-writer | Offline graph generation scripts and preprocessors for turning OSM road vectors into binary routing networks. |

---

## 3. Critical Data Flows

### A. Citizen SOS Lifecycle & Dual-Mesh Relay
The following sequence illustrates the propagation of an emergency from citizen trigger to local persistence, local mesh broadcast, multi-hop relay, and cloud synchronization:

```
[Citizen Taps SOS]
       |
       v
[SosViewModel: 5s Countdown] --(Cancel tapped)--> [Aborted]
       |
       v (Countdown expires)
[StartSosUseCase]
       |
       v
[SosRepository: Decorator Chain]
  1. SosBleBridgeRepository   -> Encodes Raw BLE GATT Packet (0x180D) & Starts BLE Advertising
  2. SosRelayRepository       -> Encodes Google Nearby Mesh Packet & Dispatches over Wi-Fi/BT
  3. SyncingSosRepository     -> Marks local record as PENDING_SYNC in SyncStateStore
  4. SosRepositoryImpl        -> Persists atomically into Local SharedPreferences / SQLite
       |
       +-----------------------------------------+
       |                                         |
       v (Local Mesh Broadcast)                  v (Uplink Available?)
[Peer Devices in 100m range]              [AndroidConnectivityMonitor]
       |                                         |
[Peer BleManager accepts packet]                 +-- YES --> [SyncRetryCoordinator]
       |                                         |                  |
[Peer saves to Local History]                    |                  v
       |                                         |           [FirebaseSosCloudDataSource]
[Peer posts Heads-up Notification]               |                  |
       |                                         |                  v
[Peer forwards packet if TTL > 1]                |           [Firebase Realtime Database:
                                                 |            /sos_alerts/{uid}/{alertId}]
                                                 |                  |
                                                 +-- NO ---> [Queued for WorkManager Sync]
```

### B. Offline Routing & Safe Route Pro Execution
The routing pipeline enables real-time navigation across catastrophic zones:

```
[Origin & Destination Selected]
       |
       v
[RouteActivity / RouteViewModel]
       |
       v
[RouteRepository.findRoute(startId, destinationId)]
       |
       v
[GeoRescueNativeRouting.findRoute]
  |
  +-- Native Engine Available? (libgeorescux_routing.so)
  |     |
  |     +-- YES -> C++ AStarPathfinder / BidirectionalAStar:
  |     |            • Haversine & Vincenty geodesic cost
  |     |            • Turn penalties & elevation impedance
  |     |            • Dynamic hazard polygon intersection
  |     |            • Blocked road exclusion (Cost = Infinity)
  |     |
  |     +-- NO  -> Pure Kotlin SafeHavenFallback:
  |                  • Kotlin AStarPathfinder over RouteGraph
  |                  • Dynamic hazard penalties applied
  |                  • Fallback to nearest reachable safe haven
  v
[EvacuationRoute: Node List, Distance, Cost, Hazard Warnings]
       |
       v
[OSMDroid MapView / MapsForge Vector Rendering]
  • Route Polyline rendered on offline vector layer
  • Warning pins placed on hazards and blocked segments
  • Turn-by-turn guidance displayed in Navigation Card
```

### C. Administrative Authorization & Incident Verification
Security enforcement guarantees that privileged operations are strictly authenticated:

```
[Admin Logs In (Email/Password)]
       |
       v
[Firebase Auth JWT Issued]
       |
       v
[FirebaseRoleResolver.resolveRole(forceRefresh = true)]
       |
       v
[Token Claims Inspected: claims["admin"] == true]
  |
  +-- FALSE -> Directed to Citizen Home / Bottom Navigation
  |
  +-- TRUE  -> Directed to AdminDashboardActivity:
                 • Subscribes to /sos_alerts (Full Read Access)
                 • Subscribes to /incidents, /hazards, /blocked_roads
                 • Can post administrative alerts to /admin_alerts
                 • Can verify citizen verification_requests
                 • Can resolve emergencies (sets status = "RESOLVED")
```

---

## 4. Key Architectural Patterns

1. **Local-First Persistence**: The application never halts or fails if a cloud service is inaccessible. Local durable storage (SQLite / SharedPreferences) is the source of truth; cloud storage is an asynchronous mirror.
2. **Repository & Decorator Pattern**: Core domain repositories (`SosRepository`, `ContactsRepository`) wrap local stores and compose additional capabilities (cloud sync, BLE bridging, mesh relay) transparently through decorated interfaces without altering client contract code.
3. **Fail-Closed Security**: In the absence of network connectivity or upon cryptographic verification failure, administrative privileges default to `UserRole.UNKNOWN`, preventing offline privilege escalation.
4. **Admissible Heuristics in A\***: Both Kotlin and C++ pathfinders use straight-line geodesic distance (Haversine/Vincenty), ensuring the A* search heuristic never overestimates the remaining distance and always guarantees the optimal evacuation path.
