# GeoRescuX: College Demonstration Checklist & Viva Guide

This document provides a structured, step-by-step guide for conducting a live academic demonstration of GeoRescuX before an examination committee, accompanied by 20 comprehensive technical viva questions and answers.

---

## Part 1 — Environment & Hardware Preparation

1. **Host Workstation:**
   - Ensure Android SDK platform-tools (`adb`) and Android Emulator are added to the system `PATH`.
   - Verify Gradle build baseline: `.\gradlew.bat testDebugUnitTest` passes 593/593 tests across 74 suites.
2. **Launch Android Virtual Device:**
   ```powershell
   & "$env:LOCALAPPDATA\Android\Sdk\emulator\emulator.exe" -avd Pixel_6 -no-snapshot-load -no-audio
   ```
   Wait until boot completes:
   ```powershell
   adb wait-for-device
   adb shell getprop sys.boot_completed  # Expected output: 1
   ```
3. **Install Release-Candidate APK:**
   ```powershell
   adb install -r -d app/build/outputs/apk/debug/app-debug.apk
   ```
4. **Confirm Authentication State:**
   - Launch app: `adb shell am start -n com.example.georescux/.MainActivity`
   - If not signed in, log in using authorized test credentials.
   - Confirm application lands cleanly on `HomeActivity`.

---

## Part 2 — Main Application Walkthrough

1. **Branded Cold Start & Role Resolution:**
   - Execute cold launch: `adb shell am force-stop com.example.georescux; adb shell am start -W -n com.example.georescux/.MainActivity`
   - **Explain to Evaluators:** Note the immediate branded loading layout (`activity_main.xml`), the progress spinner, and the asynchronous role verification taking place with a bounded 3000ms timeout (`withTimeout(3000L)`).
2. **Dashboard Overview:**
   - Point out user identity banner displaying authenticated civilian status.
   - Highlight dynamic status badges: Connectivity state (Online/Offline) and currently selected regional package.
   - Walk through bottom navigation tabs: **Home**, **Map**, **Contacts**, and **More**.
3. **Offline Emergency Contacts:**
   - Tap **Contacts** tab.
   - Demonstrate adding an emergency contact with phone number and relationship.
   - Explain that contacts are committed to local device storage first and enqueued for cloud sync.
4. **Offline Map & Regional Package:**
   - Tap **Map** tab.
   - Explain offline vector rendering and regional boundary selection (e.g., packaged Uttarakhand state map).

---

## Part 3 — SOS Emergency Lifecycle Demonstration

1. **Initiate Emergency Countdown:**
   - Return to **Home** tab.
   - Tap the prominent red **SOS** card (`buttonSos`).
   - **Point out:** The app transitions to `SosActivity`. A 2000ms grace countdown is displayed with an audible/visual pulse, preventing accidental activation.
2. **Live Active Emergency State:**
   - Once countdown concludes, show that the UI enters the **ACTIVE** state:
     - Header: `"SOS in Progress"`.
     - Timestamp: `"Started at HH:MM"`.
     - Dynamic Banner: `"Safe Route Pro Active"` (`Hazard avoidance & safe haven evacuation enabled`).
     - Live Location Status: `"ACQUIRED"` with fused latitude/longitude.
     - Note field: Optional input (capped at 120 characters) for trapped casualty counts or medical needs.
3. **Verify Non-Volatile Persistence During Emergency:**
   - Open a terminal on the host machine and run:
     ```powershell
     adb shell "run-as com.example.georescux cat /data/data/com.example.georescux/shared_prefs/sos_store.xml"
     ```
   - **Show the Evaluators:** The active emergency is physically saved on disk in `active_emergency` with `stoppedAtMs: null` and an active sequence counter.
4. **Complete the Emergency:**
   - Tap **"END EMERGENCY"** (`buttonStopEmergency`).
   - Show that the screen smoothly dismisses and returns focus to `HomeActivity`.
5. **Demonstrate Bounded History Archival:**
   - Run the storage inspection command again:
     ```powershell
     adb shell "run-as com.example.georescux cat /data/data/com.example.georescux/shared_prefs/sos_store.xml"
     ```
   - **Highlight:** `active_emergency` is now deleted from storage. The stopped emergency has been permanently moved into `sos_history` with its exact completion timestamp (`stoppedAtMs`). All previous historical records remain completely intact (bounded to 50 entries max).

---

## Part 4 — Application Force-Stop & Persistence Verification

1. **Simulate Application Process Termination:**
   - With the emergency safely completed, simulate an OS process termination:
     ```powershell
     adb shell am force-stop com.example.georescux
     ```
2. **Cold Relaunch:**
   ```powershell
   adb shell am start -W -n com.example.georescux/.MainActivity
   ```
3. **Evaluate Result:**
   - Show that the application loads immediately, resolves authorization, and displays the standard dashboard without freezing or crashing.
   - **Crucial Point:** Completed emergencies do **not** resurrect into false alarms. Active SOS state is strictly zero.
   - **Technical Distinction:** Emphasize that this test verifies resilience against application force-stop and process recreation; it does not simulate sudden physical battery extraction or flash storage corruption.

---

## Part 5 — Architectural Division: Live vs Structural Components

When presenting to evaluators, explicitly distinguish between what is running live in the emulator and what is architected in code:

| Component | Demonstration Mode | How to Explain |
|---|---|---|
| **Immediate Loading Screen & Role Router** | **Live Demonstration** | Show `MainActivity` loading screen, measure `am start -W`, inspect atomic flags and bounded timeout. |
| **SOS State Machine & Persistence** | **Live Demonstration** | Show countdown, active emergency, `sos_store.xml` inspection, and stop sequence. |
| **Fail-Closed Role Security** | **Live & Test Matrix** | Show standard user landing on Home; demonstrate 11 passing tests in `RoleRoutingCoordinatorTest.kt`. |
| **C++ Native A\* Pathfinding** | **Code & Test Matrix** | Open `AStarPathfinder.cpp`, `BidirectionalAStar.cpp`, and `GeoRescueNativeRoutingTest.kt`. Explain JNI bridge and Kotlin fallback. |
| **BLE GATT Mesh Protocol** | **Code & Architecture** | Open `GeoRescueBlePacket.kt` and `GeoRescueBleManager.kt`. Explain JSON-framed GATT characteristics. |
| **Cross-Transport Relay Bridge** | **Code & Architecture** | Open `CrossTransportRelayBridge.kt` and `DurableRelayEngine.kt`. Explain store-and-forward outbox. |
| **Firebase Cloud Sync** | **Code & Architecture** | Open `FirebaseSosCloudDataSource.kt`. Explain path `/sos_alerts/{uid}/{alertId}` and dual-homed sync. |

---

## Part 6 — Technical Viva Examination Questions & In-Depth Answers

### Q1: Why was Android/Kotlin chosen over cross-platform frameworks like Flutter or React Native?
**Answer:** GeoRescuX requires low-level, deterministic control over direct hardware radios: Bluetooth Low Energy peripheral advertising, GATT server configuration, and Google Play Services Nearby Connections. Cross-platform frameworks add abstraction layers that make background BLE duty cycling and Wi-Fi Direct socket negotiation unstable or impossible. Furthermore, Kotlin provides first-class support for C++ native integration via JNI/NDK and coroutine-based structured concurrency.

### Q2: What does the "offline-first" paradigm mean in GeoRescuX?
**Answer:** In traditional client-server apps, the cloud database is the single source of truth and local storage is merely a cache. In GeoRescuX, local device storage (SQLite/SharedPreferences) is the **primary authority**. When an emergency is created, it is committed to non-volatile local disk first. Network transmission (whether via BLE, Nearby, or Firebase) is treated as an opportunistic secondary replication step.

### Q3: Why is Firebase Realtime Database used, and what is the exact data path?
**Answer:** Firebase Realtime Database offers native local persistence caching, an optimized WebSocket transport protocol, and fine-grained declarative security rules. Its lightweight JSON tree structure is well suited for streaming real-time spatial updates. In GeoRescuX, the exact cloud path is `/sos_alerts/{uid}/{alertId}`, where `{uid}` is the authenticated user or gateway node and `{alertId}` is the emergency UUID.

### Q4: How does the SOS State Machine prevent accidental activations?
**Answer:** The SOS lifecycle is managed by `SosStateMachine.kt` as an immutable finite-state automaton. A tap transitions the system to `SosState.COUNTDOWN`, initiating a 2000ms delay accompanied by tactile and visual feedback. If the user touches cancel within this window, the event `COUNTDOWN_CANCELLED` resets the machine to `IDLE` with zero network or storage emissions.

### Q5: How is victim location acquired when cellular towers and Wi-Fi networks are down?
**Answer:** The device relies on onboard GNSS/GPS satellite receivers via the Android `FusedLocationProviderClient` with a fallback to `LocationManager.GPS_PROVIDER`. Satellite reception functions completely independently of cellular data, reading signals directly from orbital constellations (such as GPS, GLONASS, Galileo, BeiDou, and NavIC). However, actual multi-constellation support depends on the physical device hardware and chipset capabilities. (In emulator demonstrations, these signals are virtual mock coordinates).

### Q6: How does the Bluetooth Low Energy (BLE) subsystem operate in GeoRescuX?
**Answer:** The BLE subsystem uses a GATT client/server architecture (`GeoRescueBleManager.kt`, `GeoRescueBleConnection.kt`). BLE advertisements are used for device discovery. Once discovered, data is exchanged over GATT characteristics as newline-delimited UTF-8 JSON wire frames representing `GeoRescueBlePacket` objects.

### Q7: What is Google Nearby Connections and how does it fit into the architecture?
**Answer:** Nearby Connections is a peer-to-peer networking API provided by Google Play Services. It uses Bluetooth for zero-configuration discovery and dynamically negotiates high-speed, local Wi-Fi Direct or hotspot connections (`Strategy.P2P_CLUSTER`). It is utilized by GeoRescuX to transfer rich payloads—such as multi-point path histories and medical triage notes—that exceed standard BLE GATT characteristic chunk sizes.

### Q8: How does the Cross-Transport Relay Bridge work?
**Answer:** `CrossTransportRelayBridge.kt` connects the BLE GATT mesh with Google Nearby Connections. Inbound BLE packets are inspected for TTL, checked against origin, and deduplicated before being converted to `IncidentEvent` instances for Nearby cluster broadcast. Conversely, inbound Nearby events are converted to `GeoRescueBlePacket` instances and forwarded to BLE peers with decremented TTL.

### Q9: How are duplicate packets and broadcast storms prevented in the mesh?
**Answer:** GeoRescuX employs a two-tier deduplication mechanism ([CrossTransportDeduplicator](../../app/src/main/kotlin/com/example/georescux/domain/relay/CrossTransportDeduplicator.kt)):
1. An in-memory sliding-window cache of recent event UUIDs for immediate O(1) duplicate rejection.
2. A persistent SQLite seen-store (`SQLiteSeenEventStore.kt`) ensuring that even across application process restarts, previously ingested emergency events within the retention window are recognized as duplicates and suppressed from rebroadcast.

### Q10: How do Time-To-Live (TTL) and deduplication mitigate broadcast loops?
**Answer:** Packet propagation bounds and broadcast loop mitigations are enforced across both transports using distinct mechanisms rather than a universal per-node hop limit:
1. **BLE Mesh Forwarding:** In the BLE mesh subsystem (`GeoRescueBleMeshNode.kt`), each forwarding node decrements the packet's integer `ttl` field via `packet.forNextHop()`. Packets reaching `ttl <= 1` are ingested locally but not re-propagated.
2. **Cross-Transport Conversion:** During BLE ↔ Nearby cross-transport conversion (`CrossTransportRelayBridge.kt`), the carried hop budget is explicitly decremented (`remainingHops = packet.ttl - 1`).
3. **Nearby Forwarding:** Within the Google Nearby Connections relay (`RelayEngine.kt`), packet lifetime is bounded by temporal duration expiry (`isEventExpired(occurredAtMs, ttlSeconds, nowMs)`).
4. **Deduplication:** An in-memory sliding-window cache and SQLite seen-store (`CrossTransportDeduplicator`) suppress recognized duplicate event IDs within their retention window. While this mitigates broadcast storm amplification, deduplication and time-based expiry mitigate rather than mathematically guarantee the elimination of every possible cyclic broadcast across complex or partitioned topologies.

### Q11: What happens if an active emergency is created while the phone is completely offline?
**Answer:** The emergency is written immediately to `LocalSosStore` (`sos_store.xml`). Concurrently, an un-synced dirty flag is recorded in `SyncStateStore`. The app activates BLE/Nearby mesh broadcasts. The local UI functions normally. Once `AndroidConnectivityMonitor` detects internet connectivity, `SyncingSosRepository` automatically uploads the pending emergency to Firebase.

### Q12: How do synchronization retries operate without draining battery?
**Answer:** `SyncRetryCoordinator` utilizes exponential backoff with jitter to schedule retry attempts. Furthermore, long-term offline retries are delegated to Android `WorkManager` (`WorkManagerSyncScheduler`), which leverages OS-level battery constraints to execute synchronization tasks only when the device is awake and has an unmetered or active network connection.

### Q13: How does admin authorization work, and why is it considered "fail-closed"?
**Answer:** Administrative rights are verified against Firebase Auth Custom Claims (`claims["admin"] == true`) issued by a secure server. In `MainActivity`, `RoleRoutingCoordinator` executes this check asynchronously with a strict 3000ms timeout. If the claim is missing, the token is expired, or the network times out, the system fails closed: it refuses access to `AdminDashboardActivity` and falls back safely to the standard user dashboard (`HomeActivity`).

### Q14: What is Receiver-Acknowledged Custody and why is it currently a limitation?
**Answer:** Currently, `transport.send() == true` confirms only that the local operating system accepted the packet into its radio dispatch queue; the outbox entry is then cleared. It does not prove that the remote peer's radio received the bytes or that the remote application committed the alert to disk. An end-to-end receiver custody acknowledgement protocol (Phase P1.8) requires an authenticated cryptographic ACK returned from the receiver before outbox deletion.

### Q15: Why is physical multi-device testing still required despite passing 593 unit tests?
**Answer:** Unit tests verify algorithmic contracts, state machines, and mock interfaces. However, physical multi-device wireless communication is subject to real-world RF physics: 2.4 GHz signal attenuation through concrete and foliage, manufacturer-specific Android Bluetooth power-saving throttles (OEM battery optimizations like Doze mode), antenna orientation, and Wi-Fi Direct frequency contention. Physical testing is necessary to establish empirical transmission ranges.

### Q16: Why is emulator GPS data not considered real GPS validation?
**Answer:** In the Android emulator, location coordinates are synthetic values injected via virtual sensor controls. They bypass satellite Doppler shift, multi-path reflections from buildings, atmospheric ionospheric delays, and cold-start Time-To-First-Fix (TTFF) delays that real GPS chipsets experience in disaster terrain.

### Q17: What is the exact role of native C++ in GeoRescuX?
**Answer:** C++20 algorithms are implemented in `app/src/main/cpp/`:
1. `AStarPathfinder.cpp` and `BidirectionalAStar.cpp`: Offline graph search and shortest-path calculation across regional road graphs.
2. `KDTree.cpp` & `RTree.h`: Spatial indexing structures available in the native layer for geometric queries.
3. `GeoRescueNativeRoutingBridge.cpp`: JNI bridge exposing `nativeFindRoute` and `nativeFindBidirectionalRoute` to `GeoRescueNativeRouting.kt`.
4. Native execution avoids JVM memory allocations and garbage-collection latency spikes during intensive graph search. If the native shared library fails to load or link, `GeoRescueNativeRouting` catches the exception and falls back safely to pure Kotlin routing (`SafeHavenFallback.kt` / `AStarPathfinder.kt`).

### Q18: What is Safe Route Pro and how does it handle hazards?
**Answer:** Safe Route Pro is implemented via `RouteRepositoryImpl.kt`, `RoadHazard.kt`, and `AStarPathfinder.kt`. Local graph-based routing with blocked-edge avoidance and synthetic hazard penalties demonstrated in tests. Real-world hazard-feed integration and hazard accuracy remain unverified:
1. **Demonstrated Capabilities:** In automated unit tests (`SafeRouteOfflineEngineTest.kt`), the on-device pathfinder computes evacuation routes on local graphs, bypassing edges marked as blocked (`repo.setRoadBlocked(from, to, true)`) and calculating detours when synthetic hazard penalties (`RoadHazard.penaltyMeters`) are introduced.
2. **Safe Haven Classification:** Safe havens are destination nodes designated with `isSafeHaven = true` within the offline OpenStreetMap road graph data; they are not officially certified shelters.
3. **Absence of Live Telemetry:** The application does not detect or ingest live environmental conditions (such as active flooding, landslides, or road debris), nor does it integrate with real-time civil defense sensor feeds. Real-world hazard boundaries and travel safety are unverified.

### Q19: What is the current status of cryptographic authentication in the mesh?
**Answer:** Cryptographic signing is implemented in `EventSigner.kt` using Android Keystore EC P-256 with `SHA256withECDSA`, and `EventAuthenticator.kt` provides verification logic. However, in production `GeoRescuXApplication.kt`, `RelayEngine` is initialized with default `authenticator = null`, and `GeoRescueBlePacket` contains no signature field. Thus, cryptographic authentication is implemented in code but not fully wired or enforced in the production mesh path.

### Q20: What are the primary known limitations of the current release candidate?
**Answer:**
1. Physical RF validation between 3+ physical devices in a multi-hop mesh has not yet been executed.
2. End-to-end receiver custody ACK is not implemented.
3. Production mesh relay authentication is not fully wired.
4. Independent remote Firebase readback was not conducted.
5. Cold-start launch on virtual hardware takes approximately 14.7 seconds due to Dalvik/ART class verification and Firebase initialization overhead.

---
*Checklist and viva preparation guide compiled for GeoRescuX B.Tech Defense.*
