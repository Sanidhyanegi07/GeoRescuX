# GeoRescuX: Known Technical Limitations & Risk Analysis

This document provides a thorough, transparent, and honest disclosure of the current technical limitations, unverified subsystems, and engineering constraints of GeoRescuX at baseline commit `546c7690d74e2a89bff2f47164d66d767c85f849`.

---

### 1. Physical BLE Multi-Device Communication Unverified

- **Current Evidence:**
  The Bluetooth Low Energy subsystem (`ReliableBleTransport.kt`, `BlePacketCodec.kt`) is fully implemented in Kotlin and passes all automated unit tests (`BlePacketCodecTest`, `BleTransportTest`). However, tests were conducted entirely inside a single JVM process using mock objects. No physical over-the-air BLE advertisement capture between two or more physical Android phones was performed in this evaluation phase.
- **Technical Consequence:**
  Real-world BLE advertising is subject to severe platform and environmental constraints:
  - Aggressive OEM battery-optimization profiles (e.g., Xiaomi MIUI, Samsung OneUI) frequently suspend background BLE scanning or throttle advertising intervals when the screen is turned off.
  - 2.4 GHz radio frequency interference from Wi-Fi routers and microwave ovens can cause high packet error rates.
  - Hardware packet size limitations (31-byte legacy advertisement limit vs. 255-byte extended advertisements) may fail on older Android hardware that does not support Bluetooth 5.0 Advertising Extensions.
- **Required Resolution / Verification:**
  Deploy the debug APK to at least two physical Android devices with Bluetooth 5.0 support. Measure real-world packet reception rates at graduated distances (5m, 15m, 30m, 50m) across direct line-of-sight and non-line-of-sight (concrete partition) environments.

---

### 2. Physical Nearby Connections Multi-Device Communication Unverified

- **Current Evidence:**
  Google Nearby Connections integration (`NearbyConnectionsTransport.kt`, `NearbySosIngestionEngine.kt`) is verified via unit test harnesses (`NearbySosSyncTest`). Automated tests assert that endpoints are discovered and byte payloads are passed to ingestion listeners when mocked callbacks trigger. Physical peer-to-peer Wi-Fi Direct socket negotiation was not verified.
- **Technical Consequence:**
  Google Nearby Connections relies on proprietary Google Play Services background binaries. In real disaster conditions:
  - Establishing a high-speed P2P Wi-Fi Direct group between arbitrary OEM devices can take anywhere from 4 to 15 seconds.
  - Channel contention on 2.4 GHz / 5 GHz bands can abort the handshake.
  - If a device lacks Google Play Services (e.g., de-Googled ROMs, Huawei devices), this transport fails completely.
- **Required Resolution / Verification:**
  Conduct physical cluster validation across three distinct OEM hardware models (e.g., Samsung Galaxy, Google Pixel, OnePlus) in Airplane Mode with Wi-Fi and Bluetooth enabled. Measure connection negotiation latency and payload transfer completion times.

---

### 3. Multi-Hop "A → B → C → Internet" Physical Relay Undemonstrated

- **Current Evidence:**
  The cross-transport forwarding pipeline (`CrossTransportRelayBridge.kt`, `RelayEngine.kt`, `DurableRelayEngine.kt`) demonstrates correct multi-hop logic, hop decrementing, and seen-event deduplication in synthetic unit tests (`CrossTransportRelayTest`). A physical end-to-end multi-hop propagation chain where an offline Node A transmits to an offline intermediary Node B, which forwards to an internet-connected Node C that synchronizes to Firebase, has not been physically executed.
- **Technical Consequence:**
  Epidemic store-and-forward mesh topologies suffer from unpredictable delivery latency. Without physical multi-node verification:
  - True end-to-end latency across multiple hops under pedestrian movement speeds is unknown.
  - Battery drain on intermediary relay nodes continuously operating BLE scanning and Nearby advertising has not been benchmarked.
- **Required Resolution / Verification:**
  Configure a controlled physical 3-device deployment: Device A (Offline victim in basement), Device B (Offline courier moving between basement and street), Device C (Connected gateway on street with LTE). Measure total elapsed time from Device A's SOS trigger to Device C's Firebase write confirmation.

---

### 4. Receiver-Acknowledged Durable Custody (P1.8) Not Implemented

- **Current Evidence:**
  Inspection of `DurableRelayEngine.kt` confirms that when an intermediary device invokes `transport.send(peerId, payload)`, a return value of `true` is treated as a successful delivery. The intermediary node's outbox entry is subsequently marked as delivered and may be cleared.
- **Technical Consequence:**
  In Android wireless networking, `transport.send() == true` merely indicates that the local operating system kernel accepted the byte buffer into its local transmit queue. It **does not prove**:
  - That the remote peer's radio received the packets.
  - That the remote operating system delivered the bytes to the GeoRescuX application layer.
  - That the remote GeoRescuX instance successfully committed the alert to SQLite or SharedPreferences storage.
  If the link drops immediately after local submission, the packet is permanently lost ("custody drop").
- **Required Resolution / Verification:**
  Implement the Phase P1.8 Receiver-Acknowledged Protocol: Intermediary nodes must retain the emergency in `relay_outbox.db` in a `PENDING_ACK` state until an authenticated two-way cryptographic acknowledgement (`ACK_RECEIPT`) is returned by the receiver node.

---

### 5. Independent Remote Cloud Readback Not Verified

- **Current Evidence:**
  The live emulator run verified local persistence in `sos_store.xml` (`run-as com.example.georescux cat shared_prefs/sos_store.xml`) and confirmed that `SyncingSosRepository` enqueued the emergency. An independent remote readback from a secondary client or external administrative REST query directly against the live Firebase Realtime Database was not verified during this session.
- **Technical Consequence:**
  While the client-side Firebase SDK accepted the write request, network packet loss, restrictive Firebase Security Rules (`database.rules.json`), or missing cloud indexing could theoretically prevent the alert from becoming queryable by first-responder dispatch systems.
- **Required Resolution / Verification:**
  Execute an automated verification script that triggers an SOS on the mobile client and simultaneously performs an HTTP GET query against the Firebase REST API (`https://<project-id>.firebaseio.com/sos/<emergency-id>.json`) with an authorized admin bearer token to assert data parity.

---

### 6. Simulated Emulator Location Coordinates

- **Current Evidence:**
  The live smoke test verified that `LocationRepositoryImpl.kt` transitioned from `SEARCHING` to `ACQUIRED` and recorded coordinates `latitude: 39.237255, longitude: -123.1500317` with `accuracyMeters: 5`. These coordinates were injected by the Android Emulator's virtual sensor environment.
- **Technical Consequence:**
  Virtual location providers bypass all real-world GNSS physical constraints:
  - In deep mountain canyons, ravines, or collapsed concrete structures, GPS signals experience multi-path reflection or total satellite lock loss.
  - Real hardware GPS requires a Time-To-First-Fix (TTFF) that can take up to 45 seconds during cold satellite acquisition.
  - Rapid battery depletion during continuous high-accuracy GPS sampling was not measured.
- **Required Resolution / Verification:**
  Test on physical hardware outdoors in dense urban and rural terrain. Verify location fallback behavior when satellite signals are obstructed (e.g., transition to `LocationStatus.LAST_KNOWN` or degraded accuracy flags).

---

### 7. Live Admin Dashboard Session Not Evaluated in Emulator Run

- **Current Evidence:**
  The security enforcement gates (`AdminAuthGuard.kt`, `FirebaseRoleResolver.kt`, `RoleRoutingCoordinator.kt`) pass all 11 automated security tests in `RoleRoutingCoordinatorTest.kt` and `AdminAuthorizationSecurityTest.kt`. In the live emulator run, the active session belonged to `georescux.test2026@example.com`, which is a standard authenticated user account.
- **Technical Consequence:**
  The live test verified that standard users are strictly prevented from accessing the admin dashboard (fail-closed behavior). However, interactive features of `AdminDashboardActivity.kt` (such as real-time incident status modification, manual road hazard insertion, and dynamic evacuation zone broadcasting) were not exercised live in the emulator.
- **Required Resolution / Verification:**
  Configure a dedicated admin test account with `admin: true` custom claims in the Firebase Authentication console. Sign in on the emulator and perform an interactive demonstration of incident dispatch and road block management.

---

### 8. Cold-Start Application Launch Duration (~14.7 Seconds)

- **Current Evidence:**
  Android's ActivityManager reported:
  `TotalTime: 14716ms`, `WaitTime: 14720ms` during the initial cold start of `MainActivity`.
- **Technical Consequence:**
  While the branded loading screen (`activity_main.xml`) renders immediately and eliminates the critical window-focus ANR, a 14-second total launch time is sluggish. This delay is attributed to:
  - Virtual machine overhead on host hardware.
  - Dalvik/ART class loading and DEX verification across multiple multidex files (`classes.dex` through `classes18.dex`).
  - Synchronous initialization of FirebaseApp and Google Play Services client libraries in `Application.onCreate()`.
- **Required Resolution / Verification:**
  1. Profile startup using Android Studio App Startup Profiler / Simpleperf.
  2. Implement Android Jetpack `androidx.startup` library to lazily initialize non-critical components.
  3. Generate and bundle baseline profiles (`BaselineProfileRule`) to enable AOT (Ahead-Of-Time) compilation on physical devices, reducing launch latency by 30–50%.

---

### 9. Production Emergency Certification Not Established

- **Current Evidence:**
  GeoRescuX is an academic research prototype developed as a Bachelor of Technology capstone project. It has not undergone formal regulatory certification (such as FCC Part 15 wireless certification, RED compliance in the EU, or safety-critical software audits like DO-178C or ISO 26262).
- **Technical Consequence:**
  The application must not be advertised or deployed as a commercial, life-critical emergency service to the general public without formal validation and coordination with official government disaster management agencies (e.g., NDMA / SDRF).
- **Required Resolution / Verification:**
  Undergo formal independent security audits, penetration testing, and controlled field exercises in collaboration with civil defense and search-and-rescue authorities.

---
*Limitations document certified for technical accuracy and academic review.*
