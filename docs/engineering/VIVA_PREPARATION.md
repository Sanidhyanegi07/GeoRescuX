# GeoRescuX — Viva Preparation & Technical Defense Guide

## 1. Project Synopsis in 60 Seconds

**Question: "What is GeoRescuX, and why does it exist?"**

> *"GeoRescuX is an offline-first disaster response and tactical emergency evacuation ecosystem. During major natural catastrophes or civilian crises, cellular towers collapse, power grids fail, and centralized internet connections become unreachable. Traditional emergency apps crash or hang because they assume persistent cloud connectivity.
> 
> GeoRescuX solves this by making the individual smartphone completely autonomous:
> 1. All critical operations—SOS activation, GPS tracking, and Safe Route evacuation navigation—execute 100% offline using on-device SQLite storage, bundled OpenStreetMap vector maps, and an embedded C++/Kotlin A\* pathfinding engine.
> 2. Distress signals propagate horizontally through an ad-hoc, multi-hop Bluetooth Low Energy (BLE) mesh network between peer phones without needing cellular towers or Wi-Fi.
> 3. Whenever any device in the mesh temporarily regains an internet uplink (via satellite, drone, or intact cell tower), it acts as a data mule, automatically relaying all queued distress packets to emergency services and our C++ Drogon incident command center."*

---

## 2. Core Architectural Subsystems & Deep Dives

### A. The End-to-End Journey of an SOS Alert

**Question: "Explain exactly what happens from the millisecond a user taps the SOS button."**

1. **User Interaction & Countdown**:
   - The user taps the red SOS button in `SosActivity`.
   - `SosViewModel` starts a 5-second countdown timer. If the user tapped by accident, they can hit "Cancel" to abort with zero side effects.
2. **State Transition & Invariant Enforcement**:
   - When the countdown reaches zero, `StartSosUseCase` is triggered.
   - It calls `SosRepository.startEmergency(id, timestamp)`.
   - The repository enforces a critical safety invariant: **a user can have at most ONE active emergency**. If one is already active, the call returns null.
3. **Local-First Atomicity**:
   - The emergency is written atomically to durable local storage (`SharedPreferencesSosStore` in Kotlin / `SqliteEmergencyRepository` in C++).
   - This ensures that if the phone is crushed, runs out of battery, or the app process is killed 10 milliseconds later, the emergency record is permanently preserved on disk.
4. **Local Mesh Broadcast (Zero Internet Required)**:
   - `SosBleBridgeRepository` intercepts the new emergency and converts it into a binary packet using `GeoRescueBlePacketCodec`.
   - It turns on the phone's Bluetooth Low Energy transmitter in Peripheral mode and advertises GATT Service `0x180D`.
   - Simultaneously, `SosRelayRepository` uses Google Nearby Connections to broadcast an epidemic gossip message over Wi-Fi Direct and Bluetooth.
5. **Peer Relay & Citizen Notification**:
   - Nearby phones running GeoRescuX within 50–100 meters pick up the advertisement via background BLE scanning.
   - The receiving phone's `BloomFilterDeduplicator` checks whether it has already processed this packet ID. If not, it saves the alert to its own local history.
   - A high-priority heads-up notification is posted on the peer phone: *"Nearby Mesh Emergency SOS detected!"*
   - If the packet's Time-To-Live (TTL) is greater than 1, the peer phone decrements the TTL and re-advertises the packet, hopping it across the disaster zone.
6. **Cloud Synchronization (The Data Mule Effect)**:
   - When any peer device comes within range of a working cellular network or Wi-Fi hotspot, `AndroidConnectivityMonitor` detects the uplink.
   - `SyncRetryCoordinator` triggers `FirebaseSosCloudDataSource` to push all pending alerts to the Firebase Realtime Database at `/sos_alerts/{uid}/{alertId}`.
   - The emergency authorities instantly see the SOS appear on their `AdminDashboardActivity`.

---

### B. Offline Routing & Safe Route Pro

**Question: "How can the app calculate evacuation routes through a destroyed city without Google Maps or internet?"**

1. **Pre-bundled Road Topology**:
   - The app ships with bundled regional graph assets (e.g. `route_graph_uttarakhand`, ~18 MB) extracted from OpenStreetMap.
   - These assets define thousands of road intersections (Nodes) and road segments (Edges), including distance, surface type, bridge flags, and coordinates.
2. **Dynamic Hazard Penalties**:
   - A normal navigation algorithm finds the *shortest* road. Safe Route Pro finds the *safest* road.
   - Road hazards (flooded streets, landslides, collapsed bridges, wildfires) are reported by admins or corroborated by mesh reports.
   - When calculating the cost of an edge in `AStarPathfinder`:
     - If the road is marked `blocked`, its cost is set to **Infinity** (the pathfinder refuses to cross it).
     - If a hazard polygon overlaps the road, a high penalty cost (e.g. +5000 meters) is added to the edge weight.
3. **A\* Algorithm & Admissible Heuristics**:
   - The pathfinder uses the A\* algorithm with a PriorityQueue.
   - The heuristic $h(n)$ is the straight-line Haversine/Vincenty geodesic distance to the destination.
   - Because straight-line distance is always less than or equal to actual road distance, the heuristic is mathematically **admissible**, guaranteeing that the path found is optimal.
4. **Safe Haven Fallback**:
   - If an earthquake completely cuts off the user's chosen destination (e.g. every road leading to the hospital is blocked), standard navigation apps report *"No Route Found"*.
   - GeoRescuX invokes `SafeHavenFallback.findRoute()`: it automatically scans all designated community shelters, relief camps, and safe havens in the active region, finding and directing the citizen to the nearest accessible refuge.

---

### C. Security, Admin Claims & Privilege Escalation Defense

**Question: "How do you prevent an attacker from tampering with local code to declare themselves an Administrator?"**

1. **Server-Side Custom Claims**:
   - Administrator authority is never determined by a local database flag, a boolean in SharedPreferences, or a client-side setting.
   - It is governed exclusively by **Firebase Auth Custom Claims** (`auth.token.admin == true`), which can only be cryptographically signed by Google's authentication servers using a private service account key.
2. **Fail-Closed Role Resolution**:
   - In `FirebaseRoleResolver.kt`, the app calls `user.getIdToken(forceRefresh = true)`.
   - If the device is offline and the cached token is expired, or if a network error occurs, the resolver immediately returns `UserRole.UNKNOWN`. It strictly fails closed.
3. **Database Rules Enforcement**:
   - Even if an attacker decompiles the APK and modifies `AdminDashboardActivity` to display admin screens, they cannot read or write data.
   - In `database.rules.json`:
     ```json
     "hazards": {
       ".read": "auth != null",
       ".write": "auth != null && auth.token.admin == true"
     }
     ```
     Any write attempt without the signed `admin: true` JWT claim is rejected at the Firebase database protocol level with `PERMISSION_DENIED`.

---

## 3. The Four Pillars of OOP Defended with Code Evidence

### 1. Encapsulation
- **Viva Answer**: *"Encapsulation hides the internal state of an object and enforces invariants through controlled methods. In `SosRepositoryImpl.kt` (lines 13–48), the backing store `private val store: LocalSosStore` cannot be touched from outside. The class enforces that a user can never have two active emergencies at once (`if (store.loadActive() != null) return null`), and coordinate updates protect against losing previous GPS fixes when a temporary null reading occurs."*

### 2. Inheritance
- **Viva Answer**: *"Inheritance allows a class to derive properties and behavior from a parent class. In `GeoRescuXApplication.kt` (lines 353–407), our class inherits from Android's `android.app.Application`. By overriding `onCreate()` and calling `super.onCreate()`, we hook into the Android OS process lifecycle to initialize background sync and BLE services while letting the operating system set up base contexts."*

### 3. Polymorphism
- **Viva Answer**: *"Polymorphism allows different implementations to be used interchangeably through a common interface. In `GeoRescuXApplication.kt`, we use the Decorator Pattern over the `SosRepository` interface. ViewModels hold a reference to `SosRepository`. At runtime, calling `startEmergency()` dispatches through `SosBleBridgeRepository` (which triggers BLE advertising), then to `SosRelayRepository` (which triggers Nearby mesh broadcast), then to `SyncingSosRepository` (which queues cloud sync), and finally to `SosRepositoryImpl` (which writes to disk). The UI layer does not know or care which network layers are present."*

### 4. Abstraction
- **Viva Answer**: *"Abstraction hides complex implementation details behind a clean, simple contract. In `RouteRepository.kt`, the interface exposes `findRoute(startNodeId, destinationNodeId)`. The UI activity has no idea whether the route is being computed by an 18 MB graph in memory, a C++ NDK library loaded via JNI (`GeoRescueNativeRouting`), or Kotlin fallback code. It simply requests a path and receives an `EvacuationRoute`."*

---

## 4. Key Engineering Trade-Offs

| Decision | Chosen Approach | Alternative Considered | Engineering Rationale |
| :--- | :--- | :--- | :--- |
| **Dependency Injection** | Manual `AppContainer` | Dagger / Hilt | Hilt adds annotation processors, reflection overhead, and slower build times. For an emergency app, an explicit container in `GeoRescuXApplication` is fast, transparent, and completely deterministic. |
| **Map Rendering** | OSMDroid + MapsForge | Google Maps SDK | Google Maps SDK requires active internet to stream map tiles and depends on Google Play Services. MapsForge reads pre-bundled `.map` vector files directly from the APK assets offline. |
| **Mesh Networking** | Dual Transport (Raw BLE + Nearby) | Single Protocol | Google Nearby offers high bandwidth and WiFi-Direct fallback in the Android ecosystem. Raw BLE GATT (UUID `0x180D`) provides platform-agnostic broadcast to Linux, iOS, and specialized rescue hardware. |
| **Routing Language** | Dual Engine (C++ & Kotlin) | Pure Kotlin or Pure C++ | C++ gives high-speed graph searches across 100,000+ nodes. The pure Kotlin fallback guarantees Safe Route Pro still operates even if an obscure phone architecture fails to load the `.so` native library. |
