# GeoRescuX — Prioritized Implementation & Maintenance Register (TODO)

## 1. Register Overview & Status Definitions

This register categorizes all confirmed defects, pending tasks, hardware verifications, and architectural maintenance items. Each item is strictly assigned one of the following statuses:

- `VERIFIED WORKING`: Inspected, automated test passed, and verified in build.
- `DEFECT FOUND`: Defect identified with reproducible evidence; awaiting fix.
- `FIXED AND TESTED`: Defect repaired, regression tested, and validated in APK release build.
- `IMPLEMENTED — NOT PHYSICALLY VERIFIED`: Code complete and unit tested, but requires physical RF/satellite hardware for field confirmation.
- `BLOCKED BY EXTERNAL REQUIREMENT`: Requires active cloud deployment, external credentials, or production databases.
- `NOT IMPLEMENTED`: Feature designed in specifications but omitted from current source tree.
- `REQUIRES HUMAN REVIEW`: Security or architectural policy decision requiring stakeholder sign-off.

---

## 2. Prioritized Implementation & Defect Register

### Priority 1: Critical Safety & Security

| ID | Title & Module | File Path & Lines | Priority / Severity | Status | Environment & Blocker | Required Code Change & Test |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TODO-SEC-01** | Fix Self-Bootstrapping Privilege Escalation (`functions/`) | [`functions/index.js:L30-L36`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/index.js#L30-L36) | **P1 / Critical** | `FIXED AND TESTED` | Verified with Node.js unit test suite; deployment blocked by cloud credentials. | Removed `isSelfBootstrap` logic; enforced caller `token.admin === true`. Added regression test suite `functions/test/admin_security.test.js` (15/15 passing). |
| **TODO-DATA-01** | Prevent `"null"` String Corruption in Emergency Notes (`data/sos/`) | [`app/src/main/kotlin/com/example/georescux/data/sos/SosEmergencyJson.kt:L20-L40`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosEmergencyJson.kt#L20-L40) | **P1 / High** | `FIXED AND TESTED` | Fully verified in current environment. | Sanitized string decoding to ignore literal `"null"`. Tested via `SosEmergencyJsonTest`. |
| **TODO-SEC-02** | Secure Incident Photo Storage in Offline Mode (`ui/incident/`) | [`app/src/main/res/layout/bottom_sheet_report_incident.xml:L380-L420`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/bottom_sheet_report_incident.xml#L380-L420) | **P1 / High** | `IMPLEMENTED — NOT PHYSICALLY VERIFIED` | Local caching works; Cloud Storage upload blocked by Firebase storage bucket credentials. | Persist compressed image URI to local SQLite database until Firebase Cloud Storage uplink is regained. |

---

### Priority 2: Critical UI Usability & Contrast

| ID | Title & Module | File Path & Lines | Priority / Severity | Status | Environment & Blocker | Required Code Change & Test |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TODO-UI-01** | MaterialButton Background Tint Collision Fix (`themes.xml`) | [`app/src/main/res/values/themes.xml:L50-L113`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/values/themes.xml#L50-L113) | **P2 / High** | `FIXED AND TESTED` | Fully verified in current environment. | Created `Widget.GeoRescuX.Button.*` styles with explicit tints. Tested via `assembleRelease`. |
| **TODO-UI-02** | Active Tab Contrast Inversion (White on Blue) (`res/layout/`) | [`app/src/main/res/layout/activity_contacts.xml:L60-L90`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_contacts.xml#L60-L90) | **P2 / High** | `FIXED AND TESTED` | Fully verified in current environment. | Set active tab text to `@color/white` and inactive to `@color/text_muted` across 5 layouts. |
| **TODO-UI-03** | Admin Dashboard Bottom Navigation Overlap (`ui/admin/`) | [`app/src/main/res/layout/activity_admin_dashboard.xml:L30-L50`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_admin_dashboard.xml#L30-L50) | **P2 / Medium** | `FIXED AND TESTED` | Fully verified in current environment. | Added `android:paddingBottom="84dp"` and `clipToPadding="false"` to ScrollView. |
| **TODO-UI-04** | Admin Alert Card Visual Hierarchy & Dark Contrast Clash | [`app/src/main/kotlin/com/example/georescux/ui/admin/AdminDashboardActivity.kt:L200-L380`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/admin/AdminDashboardActivity.kt#L200-L380) | **P2 / Medium** | `FIXED AND TESTED` | Fully verified in current environment. | Refactored card inflation: populated `textAlertTitle`, added source badges, replaced `bg_card_dark` with `bg_card_white`. |
| **TODO-UI-05** | High-DPI Map Scaling on Offline Vector Layers (`ui/common/`) | [`app/src/main/kotlin/com/example/georescux/ui/common/OfflineMapSetup.kt:L20-L40`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/common/OfflineMapSetup.kt#L20-L40) | **P2 / Medium** | `FIXED AND TESTED` | Fully verified in current environment. | Added `mapView.isTilesScaledToDpi = true` and zoom level boundaries. |

---

### Priority 3: Hardware & Radio Verification

| ID | Title & Module | File Path & Lines | Priority / Severity | Status | Environment & Blocker | Required Code Change & Test |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TODO-HW-01** | Physical Dual-Device BLE Mesh Multi-Hop Testing (`data/ble/`) | [`app/src/main/kotlin/com/example/georescux/data/ble/GeoRescueBleManager.kt:L1-L200`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/ble/GeoRescueBleManager.kt#L1-L200) | **P3 / High** | `IMPLEMENTED — NOT PHYSICALLY VERIFIED` | Blocked by physical Bluetooth hardware and RF isolation lab. | Test GATT advertising and scanning between two physical Android devices with Bluetooth 5.0. |
| **TODO-HW-02** | GPS Accuracy & Stale Location Testing in Poor Satellite Reception | [`app/src/main/kotlin/com/example/georescux/data/location/LocationRepositoryImpl.kt:L1-L80`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/location/LocationRepositoryImpl.kt#L1-L80) | **P3 / Medium** | `IMPLEMENTED — NOT PHYSICALLY VERIFIED` | Blocked by physical GPS receiver and open-sky field testing. | Verify dead reckoning fallback when GPS accuracy exceeds 50 meters. |

---

### Priority 4: Performance & Optimization

| ID | Title & Module | File Path & Lines | Priority / Severity | Status | Environment & Blocker | Required Code Change & Test |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **TODO-PERF-01** | Binary Contraction Hierarchies Precomputation (`cpp/routing/`) | [`app/src/main/cpp/routing/ContractionHierarchies.cpp:L1-L150`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/cpp/routing/ContractionHierarchies.cpp#L1-L150) | **P4 / Medium** | `VERIFIED WORKING` | Verified in CTest suite (`tst_routing`). | Precomputes node ordering shortcuts for sub-second routing across 100,000+ road nodes. |
| **TODO-PERF-02** | Regional Graph Cache Synchronization (`data/routing/`) | [`app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt:L30-L60`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt#L30-L60) | **P4 / Low** | `VERIFIED WORKING` | Verified in JVM unit test suite. | In-memory memoization avoids repeated decoding of 18 MB graph JSON on UI thread. |
