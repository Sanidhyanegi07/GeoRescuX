# GeoRescuX: College Demonstration Checklist & Viva Guide

This document provides a structured, step-by-step guide for conducting a live academic demonstration of GeoRescuX before an examination committee, accompanied by 20 comprehensive technical viva questions and answers.

---

## Part 1 — Environment & Hardware Preparation

1. **Host Workstation:**
   - Ensure Android SDK platform-tools (`adb`) and Android Emulator are added to the system `PATH`.
   - Verify Gradle build baseline: `.\gradlew.bat testDebugUnitTest` passes 582/582 tests.
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
   - If not signed in, log in using authorized test credentials (`georescux.test2026@example.com`).
   - Confirm application lands cleanly on `HomeActivity`.

---

## Part 2 — Main Application Walkthrough

1. **Branded Cold Start & Role Resolution:**
   - Execute cold launch: `adb shell am force-stop com.example.georescux; adb shell am start -W -n com.example.georescux/.MainActivity`
   - **Explain to Evaluators:** Note the immediate branded loading layout (`activity_main.xml`), the progress spinner, and the asynchronous role verification taking place with a bounded 3000ms timeout.
2. **Dashboard Overview:**
   - Point out user identity banner: `"Logged in as: georescux.test2026@example.com"`.
   - Highlight dynamic status badges: Connectivity state (Online/Offline), current selected regional package.
   - Walk through bottom navigation tabs: **Home**, **Map**, **Contacts**, and **More**.
3. **Offline Emergency Contacts:**
   - Tap **Contacts** tab.
   - Demonstrate adding an emergency contact with phone number and relationship.
   - Explain that contacts are saved to local device storage first and queued for cloud sync.
4. **Offline Map & Regional Package:**
   - Tap **Map** tab.
   - Explain offline vector rendering and regional boundary selection (e.g., Uttarakhand or Himachal Pradesh).

---

## Part 3 — SOS Emergency Lifecycle Demonstration

1. **Initiate Emergency Countdown:**
   - Return to **Home** tab.
   - Tap the prominent red **SOS** card (`buttonSos`).
   - **Point out:** The app transitions to `SosActivity`. A 2-second grace countdown is displayed with an audible/visual pulse, preventing accidental activation.
2. **Live Active Emergency State:**
   - Once countdown concludes, show that the UI enters the **ACTIVE** state:
     - Header: `"SOS in Progress"`.
     - Timestamp: `"Started at HH:MM"`.
     - Dynamic Banner: `"Safe Route Pro Active"` (`Hazard avoidance & safe haven evacuation enabled`).
     - Live Location Status: `"ACQUIRED"` with fused latitude/longitude.
     - Note field: Optional input for trapped casualty counts or medical needs.
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
   - **Highlight:** `active_emergency` is now deleted from storage. The stopped emergency has been permanently moved into `sos_history` with its exact completion timestamp (`stoppedAtMs`). All previous historical records remain completely intact.

---

## Part 4 — Process Crash & Persistence Verification

1. **Simulate Catastrophic Process Termination:**
   - With the emergency safely completed, simulate an OS memory kill or device power failure:
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

---

## Part 5 — Architectural Division: Live vs Structural Components

When presenting to professors and examiners, explicitly distinguish between what is running live in the emulator and what is architected in code:

| Component | Demonstration Mode | How to Explain |
|---|---|---|
| **Immediate Loading Screen & Role Router** | **Live Demonstration** | Show `MainActivity` loading screen, measure `am start -W`, inspect atomic flags. |
| **SOS State Machine & Persistence** | **Live Demonstration** | Show countdown, active emergency, `sos_store.xml` inspection, and stop sequence. |
| **Fail-Closed Role Security** | **Live & Test Matrix** | Show standard user landing on Home; demonstrate 11 passing tests in `RoleRoutingCoordinatorTest`. |
| **C++ Native A\* Pathfinding** | **Code & Test Matrix** | Open `AStarPathfinder.cpp` and `NativeRouteGraphTest.kt`. Explain JNI bridge and bi-directional search. |
| **BLE Ad-Hoc Packet Mesh** | **Code & Architecture** | Open `BlePacketCodec.kt` and `ReliableBleTransport.kt`. Explain 31-byte compact advertisement structure. |
| **Cross-Transport Relay Bridge** | **Code & Architecture** | Open `CrossTransportRelayBridge.kt` and `DurableRelayEngine.kt`. Explain store-and-forward outbox. |
| **Firebase Cloud Sync** | **Code & Architecture** | Open `SyncingSosRepository.kt` and `SyncRetryCoordinator.kt`. Explain dual-homed sync and dirty flags. |

---

## Part 6 — Technical Viva Examination Questions & In-Depth Answers

### Q1: Why was Android/Kotlin chosen over cross-platform frameworks like Flutter or React Native?
**Answer:** GeoRescuX requires low-level, deterministic control over direct hardware radios: Bluetooth Low Energy peripheral advertising, GATT server configuration, and Google Play Services Nearby Connections. Cross-platform frameworks add abstraction layers that make background BLE duty cycling and Wi-Fi Direct socket negotiation unstable or impossible. Furthermore, Kotlin provides first-class support for C++ native integration via JNI/NDK and coroutine-based structured concurrency.

### Q2: What does the "offline-first" paradigm mean in GeoRescuX?
**Answer:** In traditional client-server apps, the cloud database is the single source of truth and local storage is merely a cache. In GeoRescuX, local device storage (SQLite/SharedPreferences) is the **primary authority**. When an emergency is created, it is committed to non-volatile local disk first. Network transmission (whether via BLE, Nearby, or Firebase) is treated as an opportunistic secondary replication step.

### Q3: Why is Firebase Realtime Database used instead of Firestore or a custom REST API?
**Answer:** Firebase Realtime Database offers native local persistence caching, an optimized WebSocket transport protocol, and fine-grained declarative security rules. Its lightweight JSON tree structure is ideally suited for streaming real-time spatial updates (latitude, longitude, sequence numbers) with minimal connection overhead compared to REST round-trips.

### Q4: How does the SOS State Machine prevent accidental activations?
**Answer:** The SOS lifecycle is managed by `SosStateMachine.kt` as an immutable finite-state automaton. A single tap transitions the system to `SosState.COUNTDOWN`, initiating a 2000ms delay accompanied by tactile and auditory feedback. If the user touches cancel within this window, the event `COUNTDOWN_CANCELLED` resets the machine to `IDLE` with zero network or storage emissions.

### Q5: How is victim location acquired when cellular towers and Wi-Fi networks are down?
**Answer:** The device relies on onboard GNSS/GPS satellite receivers via the Android `FusedLocationProviderClient` with a fallback to `LocationManager.GPS_PROVIDER`. Satellite reception functions completely independently of cellular data, reading signals directly from orbital GPS, GLONASS, Galileo, and BeiDou constellations.

### Q6: How does the Bluetooth Low Energy (BLE) subsystem operate without device pairing?
**Answer:** Traditional Bluetooth requires PIN pairing and RFCOMM connection establishment. GeoRescuX utilizes BLE **Non-Connectable Manufacturer Advertising**. The emergency payload (UUID, origin hash, coordinates, timestamp, TTL) is packed into a compact binary format ([BlePacketCodec](file:///C:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/relay/BlePacketCodec.kt)) and broadcast directly within advertisement packets. Surrounding devices in continuous passive scanning mode receive the alert without manual pairing.

### Q7: What is Google Nearby Connections and how does it fit into the architecture?
**Answer:** Nearby Connections is a peer-to-peer networking API provided by Google Play Services. It uses Bluetooth for zero-configuration discovery and dynamically negotiates high-speed, local Wi-Fi Direct or hotspot connections (`Strategy.P2P_CLUSTER`). It is utilized by GeoRescuX to transfer rich payloads—such as multi-point path histories and medical triage notes—that exceed BLE advertisement size limits.

### Q8: How does the Cross-Transport Relay Bridge work?
**Answer:** In a disaster, some devices only support BLE advertising while others support high-speed Nearby clusters. `CrossTransportRelayBridge.kt` listens for packets received on any transport. Upon validating and persisting the packet, it translates the data model into the destination transport's format and queues it for retransmission across alternative radio interfaces, allowing heterogeneous devices to bridge disjoint network segments.

### Q9: How are duplicate packets and broadcast storms prevented in the mesh?
**Answer:** GeoRescuX employs a two-tier deduplication mechanism ([CrossTransportDeduplicator](file:///C:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/relay/CrossTransportDeduplicator.kt)):
1. An in-memory sliding-window cache of recent event UUIDs for immediate O(1) rejection.
2. A persistent SQLite seen-store (`SQLiteSeenEventStore.kt`) ensuring that even across app restarts, previously ingested emergency events are never rebroadcast.

### Q10: How does the Time-To-Live (TTL) / Hop Limit prevent infinite loops?
**Answer:** Every emergency packet includes an integer `hopCount` field (typically initialized to 3 or 5). Every relay node that receives and forwards the packet decrements the `hopCount` by 1. When `hopCount <= 0`, the packet is dropped and not re-advertised, bounding the geographical spread and eliminating cyclic broadcast amplification.

### Q11: What happens if an active emergency is created while the phone is completely offline?
**Answer:** The emergency is written immediately to `LocalSosStore` (`sos_store.xml`). Concurrently, an un-synced dirty flag is recorded in `SyncStateStore`. The app activates BLE/Nearby mesh broadcasts. The local UI functions normally. Once `AndroidConnectivityMonitor` detects internet connectivity, `SyncingSosRepository` automatically uploads the pending emergency to Firebase.

### Q12: How do synchronization retries operate without draining battery?
**Answer:** `SyncRetryCoordinator` utilizes exponential backoff with jitter to schedule retry attempts. Furthermore, long-term offline retries are delegated to Android `WorkManager` (`WorkManagerSyncScheduler`), which leverages OS-level battery constraints to execute synchronization tasks only when the device is awake and has an unmetered or active network connection.

### Q13: How does admin authorization work, and why is it considered "fail-closed"?
**Answer:** Administrative rights are verified against Firebase Auth Custom Claims (`claims["admin"] == true`) issued by a secure server. In `MainActivity`, `RoleRoutingCoordinator` executes this check asynchronously with a strict 3000ms timeout. If the claim is missing, the token is expired, or the network times out, the system fails closed: it refuses access to `AdminDashboardActivity` and falls back safely to the standard user dashboard (`HomeActivity`).

### Q14: What is Receiver-Acknowledged Custody and why is it important?
**Answer:** In the current build, when `transport.send()` returns `true`, it confirms only that the local operating system accepted the packet into its radio buffer. It does not prove that the remote device received the packet or committed it to disk. A full receiver-acknowledged custody protocol (Phase P1.8) requires the receiving device to send back an authenticated cryptographic ACK before the sending device deletes the alert from its outbox queue.

### Q15: Why is physical multi-device testing still required despite passing 582 unit tests?
**Answer:** Unit tests verify algorithmic contracts, state machines, and mock interfaces. However, physical multi-device wireless communication is subject to real-world RF physics: 2.4 GHz signal attenuation through concrete and foliage, manufacturer-specific Android Bluetooth power-saving throttles (OEM battery optimizations like Doze mode), antenna orientation, and Wi-Fi Direct frequency contention. Physical testing is necessary to establish empirical transmission ranges.

### Q16: Why is emulator GPS data not considered real GPS validation?
**Answer:** In the Android emulator, location coordinates are synthetic values injected via the `geo fix` interface or Android Studio's virtual sensor controls. They bypass satellite Doppler shift, multi-path reflections from buildings, atmospheric ionospheric delays, and cold-start Time-To-First-Fix (TTFF) delays that real GPS chipsets experience in mountain valleys.

### Q17: What is the exact role of native C++ in GeoRescuX?
**Answer:** C++ is utilized for compute-intensive spatial and routing operations:
1. `AStarPathfinder.cpp`: Bi-directional A* search across large regional road graphs.
2. `KDTree.cpp` & `RTree.h`: Rapid spatial indexing to find nearest road intersections and test if an evacuation segment intersects dynamic hazard polygons.
3. Native execution avoids JVM memory allocations and garbage-collection latency spikes during emergency path recalculation.

### Q18: What is Safe Route Pro and how does it differ from standard navigation?
**Answer:** Standard navigation optimizes solely for travel time or distance along unobstructed roadways. Safe Route Pro evaluates dynamic disaster hazards (e.g., active flood zones, landslide debris, bridge collapses). It treats hazards as topological exclusion zones or high-cost penalties in the routing graph, computing paths that route civilians away from danger zones toward certified safe havens.

### Q19: How are emergency alerts authenticated against tampering?
**Answer:** In-flight packets are signed using an HMAC-SHA256 signature generated by `EventAuthenticator.kt` using a shared key or device credential. Intermediate relay nodes verify the signature before forwarding; packets with tampered coordinates or invalid timestamps are rejected, protecting the mesh against denial-of-service and false panic attacks.

### Q20: What are the primary known limitations of the current release candidate?
**Answer:**
1. Physical RF validation between 3+ physical devices in a multi-hop mesh has not yet been executed; testing has been performed via unit test suites and single-instance emulator verification.
2. End-to-end receiver custody ACK is an architectural design not yet implemented in the client code.
3. Cold-start launch on virtual hardware takes approximately 14 seconds due to Dalvik/ART class verification and Firebase initialization overhead.

---
*Checklist and viva preparation guide compiled for GeoRescuX B.Tech Defense.*
