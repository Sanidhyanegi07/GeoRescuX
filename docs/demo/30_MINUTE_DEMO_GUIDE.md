# GeoRescuX — 30-Minute BTech Faculty Demonstration Guide

## 1. Executive Demonstration Plan & Timed Schedule

This document serves as the operational script and technical defense guide for a 30-minute faculty examination of the **GeoRescuX Life-Safety Disaster Response Platform** on a physical Android device.

### 30-Minute Chronological Breakdown

| Time Window | Segment | Primary Screen / Component | Demonstration Objective & Core Concept |
| :---: | :--- | :--- | :--- |
| **00:00 – 04:00**<br>(4 min) | **System Overview & Problem Statement** | Slide / Architecture Diagram & Physical Device Handset | Explain the zero-connectivity disaster response paradigm: cellular tower destruction, power grid failure, and the necessity of offline-first local persistence + C++ A* routing. |
| **04:00 – 08:00**<br>(4 min) | **Authentication, Security & Role Resolution** | [`LoginActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/auth/LoginActivity.kt) → [`HomeActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/home/HomeActivity.kt) | Demonstrate fail-closed role resolution. Explain Firebase custom claim `{ admin: true }` vs normal citizen. Show live connectivity badge transitioning from ONLINE to OFFLINE. |
| **08:00 – 14:00**<br>(6 min) | **Offline Map & A* Hazard-Aware Routing** | [`MapActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/map/MapActivity.kt) → [`RouteActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/routing/RouteActivity.kt) | **Engage Airplane Mode (Zero Connectivity)**. Demonstrate 100% offline vector rendering via MapsForge/osmdroid, place name search, safe haven routing, and dynamic blocked-road recalculation. |
| **14:00 – 20:00**<br>(6 min) | **Emergency SOS Lifecycle & Local Persistence** | [`SosActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/sos/SosActivity.kt) & [`ActiveSosBanner`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/common/ActiveSosBanner.kt) | Trigger a safe test SOS. Demonstrate the 2-second cancel window, non-trapping active emergency state, telemetry capture, safe notes input, and persistent banner across the app. |
| **20:00 – 24:00**<br>(4 min) | **Store-and-Forward Sync & Admin Command Portal** | [`AdminDashboardActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/admin/AdminDashboardActivity.kt) | **Disable Airplane Mode (Restore Uplink)**. Demonstrate automatic background synchronization of queued emergency records via `SyncRetryCoordinator` and inspection from the Admin command view. |
| **24:00 – 27:00**<br>(3 min) | **Hardware Boundaries & BLE Scope Clarity** | [`BleDiagnosticsActivity`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/ble/BleDiagnosticsActivity.kt) | Present the radio-frequency architecture honestly: point-to-point BLE GATT & Google Nearby Connections status. Clearly disclaim multi-hop as unit-tested but explicitly deferred from live RF demo. |
| **27:00 – 30:00**<br>(3 min) | **Faculty Q&A, OOP Architecture & Defense** | Code Inspection / Architecture Documentation | Answer examiners' questions regarding OOP pillars, battery limits, database security rules, and C++/Kotlin JNI integration. |

---

## 2. Exact Screen Sequence & Navigation Map

```
                    ┌─────────────────────────┐
                    │      MainActivity       │
                    └────────────┬────────────┘
                                 │
                     [isSignedIn? / Role?]
                     ┌───────────┴───────────┐
                     ▼                       ▼
            ┌─────────────────┐     ┌────────────────────────┐
            │  LoginActivity  │     │ AdminDashboardActivity │ (if admin == true)
            └────────┬────────┘     └────────────────────────┘
                     │ (sign in as citizen)
                     ▼
            ┌────────────────────────────────────────────────────────┐
            │                     HomeActivity                       │
            │  - Live Connectivity Badge (ONLINE/OFFLINE)            │
            │  - Offline BLE Relay Diagnostics Card                  │
            │  - Single-Tap SOS Countdown Trigger                    │
            └──────┬─────────────┬─────────────┬──────────────┬──────┘
                   │             │             │              │
                   ▼             ▼             ▼              ▼
           ┌──────────────┐┌───────────┐┌──────────────┐┌──────────────┐
           │ MapActivity  ││ Contacts  ││ RouteActivity││ SosActivity  │
           │ (Offline OSM)││ Activity  ││ (A* Engine)  ││ (Emergency)  │
           └──────┬───────┘└───────────┘└──────────────┘└──────┬───────┘
                  │                                            │
                  └───────────────► ActiveSosBanner ◄──────────┘
                                (Global Non-Trapping Banner)
```

---

## 3. Turn-by-Turn Speaking Script for Each Screen

### Screen 1: App Launch & Login (`LoginActivity`)
- **Action**: Open the GeoRescuX app on the physical device.
- **Presenter Script**:
  > *"Respected examiners, GeoRescuX is an offline-first life-safety disaster management platform built specifically for scenarios where cellular towers, power lines, and cloud backends have failed.
  >
  > When the application boots, `MainActivity` performs a fail-closed role check using `FirebaseRoleResolver`. Rather than trusting local shared preferences or insecure client-side flags, the system forces a token refresh to inspect cryptographically signed custom claims. If `{ admin: true }` is present, it launches the Command Portal; otherwise, it presents the Citizen interface. Let us sign in with our test citizen account."*

### Screen 2: Citizen Dashboard (`HomeActivity`)
- **Action**: Point out the top connection badge and the middle mesh relay card.
- **Presenter Script**:
  > *"Notice the status indicators on this main dashboard:
  > 1. At the top right, our `ConnectivityBadge` informs the user of active uplink status. Notice that when we toggle Airplane mode, the badge turns amber and clearly states: 'OFFLINE — map, Safe Route and SOS fully functional'. The user is never blocked or presented with an error modal.
  > 2. In the center, our Offline Relay Card reports Bluetooth status, active scanning/broadcasting states, and pending synchronization queue size.
  > 3. Below, four core modules are exposed: Emergency Contacts, Safe Evacuation Routing, Offline Maps, and the prominent SOS emergency trigger."*

### Screen 3: Emergency Contacts & Helplines (`ContactsActivity`)
- **Action**: Tap 'Emergency Contacts'. Switch between 'My Contacts' and 'Emergency Services'.
- **Presenter Script**:
  > *"In an emergency, users must reach first responders with zero friction. The Emergency Services tab pre-loads verified national response helplines: 112 (National Emergency), 108 (Disaster Medical), and 1078 (NDMA). Tapping any service immediately opens the native device dialer via `Intent.ACTION_DIAL`.
  > Under 'My Contacts', personal primary contacts are stored in local encrypted persistence and backed up to Firebase Realtime Database with strict user-isolated security rules under `/contacts/{uid}`."*

### Screen 4: Offline Map & Regional Boundaries (`MapActivity`)
- **Action**: Open the 'Map' tab. Pan and zoom across the regional boundary box.
- **Presenter Script**:
  > *"We will now activate Airplane Mode to prove zero-connectivity capability.
  >
  > Notice that the map renders instantly without cellular data or Wi-Fi. This is powered by an offline vector tile layer configured in `OfflineMapSetup.kt` using MapsForge and osmdroid.
  > The purple bounding polygon marks the boundaries of the pre-loaded regional disaster package—in this case, Uttarakhand (covering 30°N–31.5°N, 78°E–80°E).
  > Users can also report localized incidents such as landslides, flash floods, or road blocks via the 'Report Incident' bottom sheet, which pins the GPS coordinates and stores them locally."*

### Screen 5: Safe Route Offline Evacuation Engine (`RouteActivity`)
- **Action**: Open 'Safe Evacuation Route'. Select a start point and destination (or safe haven hospital).
- **Presenter Script**:
  > *"During floods or earthquakes, standard navigation tools fail because they rely on cloud servers and do not know about road collapse.
  >
  > GeoRescuX features an offline A\* pathfinding engine (`AStarPathfinder.kt`) operating over a topological road graph. Green crosses represent safe havens (designated shelters and disaster hospitals).
  > When I select a safe haven, A\* calculates the shortest path in milliseconds completely on-device.
  > Furthermore, notice how the engine handles road hazards: if a road segment is marked with a hazard or completely blocked, our router applies dynamic penalty costs or marks the edge impassable (cost = Infinity), instantly recalculating an alternative detour."*

### Screen 6: SOS Activation & Non-Trapping Lifecycle (`SosActivity`)
- **Action**: Return to Home. Press the SOS button. Let the 2-second countdown elapse into ACTIVE state.
- **Presenter Script**:
  > *"Now we demonstrate the core life-safety emergency flow.
  >
  > When the user initiates an SOS, `SosStateMachine` enters a 2-second countdown. If tapped accidentally, tapping anywhere immediately cancels the countdown with zero false alarms.
  > Once the 2 seconds elapse, the emergency becomes ACTIVE:
  > 1. A unique, immutable emergency ID is generated and saved locally to disk in `SosRepositoryImpl`.
  > 2. GPS telemetry acquisition begins immediately. Notice the status indicator: if GPS is still locking onto satellites, it honestly displays 'Acquiring GPS fix' rather than fabricating fake coordinates.
  > 3. The victim can enter a distress note (e.g., 'Flooding on ground floor, 2 persons'). Note that our JSON serializer strictly sanitizes inputs, preventing corrupted 'null' strings.
  > 4. Crucially, the user is NOT trapped on this screen. If I press the Back button, the emergency remains ACTIVE in the background, and a prominent red banner appears at the top of every screen in the app. Tapping that banner returns us directly to the emergency."*

### Screen 7: Cloud Synchronization & Network Regain (`SyncRetryCoordinator`)
- **Action**: Turn Airplane Mode OFF.
- **Presenter Script**:
  > *"Now observe what happens when uplink connectivity is regained.
  >
  > While in Airplane mode, the active emergency was queued durably in `SyncStateStore`. The moment network capabilities are validated, `SyncRetryCoordinator` receives a network callback, automatically uploads the emergency record to `/sos_alerts/{uid}`, and clears the pending retry flag. The app functions as a reliable store-and-forward node."*

### Screen 8: Incident Resolution & Completion
- **Action**: Return to `SosActivity` and tap 'Stop Emergency'.
- **Presenter Script**:
  > *"When the victim is safe, pressing 'Stop Emergency' transitions the state machine to COMPLETED, recording the exact `stoppedAtMs` epoch timestamp. The emergency is closed in the repository, and the persistent banner dismisses automatically."*

---

## 4. Safe Test-SOS Procedure

To prevent accidental emergency dispatch or confusion during the faculty demonstration:

1. **Pre-populate the Distress Note**:
   Type: `"BTech Academic Demonstration - Test Simulation - No Distress"`.
2. **Observe GPS Honesty**:
   Point out that the coordinates displayed match the current classroom or show accurate accuracy bounds (e.g., `±12m`).
3. **Do Not Trigger Real Service Helplines**:
   In `ContactsActivity`, tap a service to show that the system opens the Android phone dialer with the number pre-filled, but **do not press the green Call button**.
4. **Demonstrate Cancellation**:
   Perform one quick test where you tap SOS and immediately tap 'Cancel' during the 2-second countdown to prove false-positive prevention.
5. **Clean Up**:
   Always tap 'Stop Emergency' at the end of the demonstration so the device does not retain an active emergency flag.

---

## 5. GPS and Offline Map Demonstration Steps

1. **Demonstrating 100% Offline Capability**:
   - Swipe down the Android Quick Settings drawer.
   - Tap **Airplane Mode** to disable Cellular and Wi-Fi.
   - Return to GeoRescuX: point out the top `ConnectivityBadge` showing `● OFFLINE`.
   - Open **Map**: pan around Dehradun / Rishikesh / Haridwar; zoom in to level 16 to show offline road vector rendering.
2. **Demonstrating Offline Place Search**:
   - Open **Safe Route**.
   - In the Destination field, begin typing a known place name (e.g., `"Dehradun"` or `"AIIMS"`).
   - Point out the instant autocomplete dropdown powered by the offline `places.json` index (zero network calls).
3. **Demonstrating Hazard Detour**:
   - Find a route between two nearby graph nodes.
   - Point out the calculated distance and estimated evacuation time.
   - Explain how `HazardEventRouteBridge` dynamically updates edge weights when incoming reports flag landslides or bridge failures.

---

## 6. Firebase Live & Cloud Evidence to Prepare

Have a laptop or second device ready with the Firebase Console open to project `georescex`:

1. **Realtime Database Browser**:
   - Navigate to `/sos_alerts`.
   - Show how the victim's emergency record appears under `/sos_alerts/{user_uid}/{alert_id}`.
   - Point out the schema fields: `id`, `startedAtMs`, `stoppedAtMs`, `status: "ACTIVE"`, `latitude`, `longitude`, `note`.
2. **Security Rules Verification**:
   - Open [`database.rules.json`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/database.rules.json).
   - Show examiners line 12: root `/sos_alerts` read access requires `auth.token.admin == true`, proving that normal citizens cannot snoop on other citizens' distress data.
3. **Cloud Function Security Gate**:
   - Open [`functions/index.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/index.js).
   - Show the 15 passing security tests in [`functions/test/admin_security.test.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/test/admin_security.test.js) which prevent self-bootstrapping privilege escalation.

---

## 7. BLE Multi-Hop: Explicit Demonstration Scope Disclaimer

**Crucial Statement to Deliver Honestly to the Faculty:**

> *"Respected committee, our communication architecture includes two layers:
> 1. Long-range store-and-forward cloud synchronization for wide-area relief coordination.
> 2. Local-area peer-to-peer device discovery via Bluetooth Low Energy (BLE) and Google Nearby Connections.
>
> In this live demonstration, we are showcasing the local-first persistence, offline vector routing, and cloud store-and-forward pipeline.
> Multi-hop BLE packet forwarding across three or more physical phones in an active RF environment requires specialized RF shielding and multi-device radio staging. While the packet framer, Bloom filter anti-replay guard, and GATT state machine are implemented and verified via automated test suites (such as `GeoRescueBleMeshNodeTest` and `SlidingWindowReplayGuard`), live multi-hop forwarding across arbitrary physical devices is explicitly outside today's live demonstration scope."*

This honest, rigorous presentation demonstrates high engineering maturity and avoids catastrophic live RF demo failures.

---

## 8. Backup Evidence & Failure Contingency Plan

If classroom conditions cause unexpected problems:

| Potential Failure Scenario | Immediate Contingency Action | Backup Artifact to Present |
| :--- | :--- | :--- |
| **No GPS fix inside concrete building** | Explain indoor satellite attenuation; show that the app gracefully falls back to cached coordinates or indicates `"Acquiring GPS fix"` without crashing. | Show unit test results: `SosLocationPolicyTest` and `LocationTest` (CTest). |
| **No Wi-Fi / Hotspot for Firebase** | Keep phone in Airplane Mode and explain that GeoRescuX is designed specifically for this reality. All local SOS and routing features remain 100% operational. | Show `SyncStateStore` local persistence logic and local JSON dump. |
| **Map tiles take time to load** | Switch to the sample region or pre-cached Uttarakhand bounds. | Show `UttarakhandRegionGraphTest` verifying 1,000+ nodes loaded in memory. |
| **Physical phone battery low** | Connect via USB and project screen using `scrcpy` (`scrcpy --stay-awake`). | Video recording of complete flow saved on laptop. |

---

## 9. Tough Faculty Questions & Bulletproof Technical Answers

### Q1: "Why did you build custom offline routing instead of using the Google Maps SDK?"
> **Answer**: *"Google Maps Android SDK strictly requires an active internet connection to download vector tiles and query the Directions API. In a cloudburst, earthquake, or cyclone, telecommunication towers lose power within hours. GeoRescuX uses an embedded A\* pathfinder operating over a locally bundled topological graph parsed from OpenStreetMap data, coupled with MapsForge offline tile rendering. The app calculates routes completely disconnected from the internet."*

### Q2: "How do you prevent race conditions and duplicate emergencies if the user presses SOS repeatedly?"
> **Answer**: *"In `SosRepositoryImpl.kt`, active emergencies are guarded by a single-active invariant: if `getActiveEmergency()` is non-null, any subsequent activation request is ignored or returns the existing active record. Furthermore, the UI enforces a state machine (`SosStateMachine`) where repeated taps are debounced, and the database synchronization uses idempotent keys based on the unique emergency UUID."*

### Q3: "How does GeoRescuX adhere to the Four Pillars of Object-Oriented Programming?"
> **Answer**:
> - **Encapsulation**: Repositories (e.g., `SosRepositoryImpl`) encapsulate raw file I/O, SQLite access, and JSON serialization behind private fields; external consumers interact only through controlled domain use cases (`StartSosUseCase`, `StopSosUseCase`).
> - **Abstraction**: Clear interfaces like `LocationRepository`, `AuthRepository`, and `RouteRepository` define behavior without exposing whether data comes from GPS, fused location, mock fixtures, or disk.
> - **Inheritance**: Activities inherit from Android's `AppCompatActivity` and ViewModels from `ViewModel`, extending lifecycle behavior; C++ routers inherit from base graph models.
> - **Polymorphism**: The sync layer uses polymorphism via repository decorators: `SyncingSosRepository` implements `SosRepository` while wrapping the local implementation to transparently add background cloud dispatch.

### Q4: "What happens if the Android operating system kills the app in the background during an emergency?"
> **Answer**: *"First, `SosActivity` launches `GeoRescueBleForegroundService` with an ongoing foreground notification, which elevates the process priority and prevents Android's Low Memory Killer (LMK) from killing it. Second, the emergency state is persisted immediately to disk upon creation. When the user re-opens the app, `SosActivity` reads the active emergency from disk and instantly restores the active countdown/distress state."*

### Q5: "How do you protect admin accounts from being compromised?"
> **Answer**: *"We remediated the critical self-bootstrapping vulnerability in `functions/index.js` (FINDING-SEC-01). Ordinary users cannot grant themselves admin status. Admin claims can only be signed through Google's backend servers using our secure offline provisioning script `georescux-admin-provision/manageAdmin.js` using service account private keys. Furthermore, Firebase database rules strictly enforce `auth.token.admin == true` for writing alerts and reading distress records."*

---

## 10. Final Pre-Demo Physical Phone Checklist (T-minus 30 Minutes)

- [ ] **Handset Battery**: Charged above 70%.
- [ ] **Display Settings**: Screen timeout set to 10 minutes (or Stay Awake in Developer Options).
- [ ] **App Permissions Granted**: Fine Location, Nearby Devices, Notifications, Bluetooth.
- [ ] **Test Accounts Verified**:
  - Citizen test account: `user@georescux.com` / `Password123!`
  - Admin test account (if showing admin view): Verified in Firebase Auth.
- [ ] **Offline Map Data Verified**: Uttarakhand or sample region package pre-cached in `assets/maps/`.
- [ ] **Dialer Integration Tested**: Tapping 112 opens dialer without crashing.
- [ ] **Airplane Mode Tested**: Confirmed map and routing work with cellular/Wi-Fi turned off.
- [ ] **Laptop Backup Open**: Firebase Console and architecture documentation ready on projection screen.
- [ ] **Audio/Haptics**: Phone volume set to a reasonable level for feedback sounds.
