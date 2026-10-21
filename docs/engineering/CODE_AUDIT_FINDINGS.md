# GeoRescuX — Code Audit Findings & Vulnerability Register

## 1. Audit Methodology & Scope

A function-by-function, line-by-line inspection was conducted across the entire GeoRescuX repository:
- **Android Kotlin Production App** (`app/src/main/kotlin/...`, XML resources, themes, drawables)
- **C++ Shared Native Engine** (`app/src/main/cpp/...`)
- **Native Qt C++ Prototype** (`prototype_qt/...`)
- **Drogon C++ Backend** (`backend/...`)
- **Firebase Security Rules & Cloud Functions** (`database.rules.json`, `functions/...`, `georescux-admin-provision/...`)

Every actionable finding is classified by severity (**Critical**, **High**, **Medium**, **Low**), with root cause, observable consequence, remediation, regression testing, and verification status.

---

## 2. Confirmed Defects & Security Vulnerabilities

### FINDING-SEC-01: Self-Bootstrapping Privilege Escalation Vulnerability
- **Severity**: **Critical**
- **Location**: [`functions/index.js:L30-L37`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/index.js#L30-L37)
- **Subsystem**: Firebase Cloud Functions / Admin Claim Authorization
- **Root Cause**:
  In `functions/index.js`, the `setAdminClaim` callable Cloud Function contains the following logic:
  ```javascript
  const callerIsAdmin = context.auth.token?.admin === true;
  const isSelfBootstrap = targetUid === context.auth.uid;
  if (!callerIsAdmin && !isSelfBootstrap) {
    throw new functions.https.HttpsError(
      "permission-denied",
      "Only administrators may manage another user's administrator privileges."
    );
  }
  ```
  The boolean `isSelfBootstrap` evaluates to `true` whenever any signed-in user calls the function with their own UID (`targetUid === context.auth.uid`). As a result, the permission check passes even if the caller has no administrative privileges.
- **Observable Consequence**:
  Any registered citizen or malicious actor could invoke `firebase.functions().httpsCallable("setAdminClaim")({ targetUid: myUid, admin: true })` and grant themselves administrative authority on the Firebase project.
- **Remediation**:
  Remove `isSelfBootstrap` or restrict self-bootstrapping to a single pre-configured administrative master email/UID stored in environment variables (`process.env.BOOTSTRAP_ADMIN_UID`). In production, admin provisioning must only be performed out-of-band using [`georescux-admin-provision/makeAdmin.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/georescux-admin-provision/makeAdmin.js) via the Firebase Admin SDK and service account credentials.
- **Verification Status**: `DEFECT CONFIRMED (Documentation/Security Advisory)`. Cloud function is currently un-deployed.

---

### FINDING-UI-01: MaterialButton Theme Tint Collision (Blue-on-Blue Inverted Text)
- **Severity**: **High**
- **Location**: [`app/src/main/res/values/themes.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/values/themes.xml), [`app/src/main/res/layout/activity_admin_dashboard.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_admin_dashboard.xml), [`activity_route.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_route.xml), [`activity_contacts.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_contacts.xml), [`activity_alerts.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_alerts.xml)
- **Subsystem**: Android Presentation Layer / Material Components
- **Root Cause**:
  In `Theme.MaterialComponents.DayNight.NoActionBar`, `<Button>` XML tags are automatically inflated as `MaterialButton`. AGP injected `@color/brand_primary` (`#0066CC`) as the default `backgroundTint`. This overrode custom drawables (e.g. `@drawable/bg_button_white`), turning white secondary buttons, danger buttons, and transparent icon back buttons into solid blue rectangles. Because their text colors were set to `@color/brand_primary`, the text rendered `#0066CC` on `#0066CC` (0% contrast).
- **Observable Consequence**:
  Buttons appeared completely blank, illegible, or blue-on-blue under high-stress conditions. In Admin Dashboard, "Log System Event" and "Manage Users" buttons appeared as solid blue blocks with invisible text.
- **Minimal Safe Fix**:
  Created dedicated styles in `themes.xml`:
  - `Widget.GeoRescuX.Button` (Primary: brand_primary background, white text)
  - `Widget.GeoRescuX.Button.Secondary` (Outlined: white background, subtle stroke, brand_primary text)
  - `Widget.GeoRescuX.Button.Danger` (Emergency red background, bold white text)
  - `Widget.GeoRescuX.Button.Danger.Outlined` (White background, red stroke, red text)
  - `Widget.GeoRescuX.Button.Icon` (Borderless, 44dp touch target, transparent background)
  - `Widget.GeoRescuX.Button.MapControl` (Floating 40x40dp white square with border and subtle elevation)
  Applied these styles across all layout XML files.
- **Verification Status**: `FIXED AND TESTED`. Verified in release APK build.

---

### FINDING-DATA-01: Literal `"null"` String Corruption in Emergency Notes
- **Severity**: **High**
- **Location**: [`app/src/main/kotlin/com/example/georescux/data/sos/SosEmergencyJson.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosEmergencyJson.kt), [`SosRepositoryImpl.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt), [`AdminDashboardActivity.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/admin/AdminDashboardActivity.kt)
- **Subsystem**: SOS Telemetry & JSON Serialization
- **Root Cause**:
  When an emergency record without a note was serialized or read from Firebase snapshots, `.optString("note")` returned `"null"` or Kotlin string interpolation transformed `null` into the 4-character string `"null"`.
- **Observable Consequence**:
  Citizen SOS screens and administrative distress cards displayed `"Note: null"` instead of hiding the note or showing `"No notes attached"`.
- **Minimal Safe Fix**:
  Sanitized note handling in `SosEmergencyJson.kt`:
  ```kotlin
  val note = obj.optString("note", "")
      .trim()
      .takeIf { it.isNotEmpty() && !it.equals("null", ignoreCase = true) }
  ```
  And added matching guards in `SosRepositoryImpl.kt` and `AdminDashboardActivity.kt`.
- **Verification Status**: `FIXED AND TESTED`. Validated by `SosEmergencyJsonTest` unit test.

---

### FINDING-UI-02: Active Tab Contrast Inversion (Blue Text on Blue Background)
- **Severity**: **High**
- **Location**: [`app/src/main/res/layout/activity_contacts.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_contacts.xml), [`activity_alerts.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_alerts.xml), [`activity_region_selection.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_region_selection.xml), [`activity_login.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_login.xml)
- **Subsystem**: Tab Navigation / Filter Chips
- **Root Cause**:
  Tab containers applied `@drawable/bg_tab_active` (`#0066CC`), while layout XML and Kotlin click listeners explicitly applied `textColor = @color/brand_primary` (`#0066CC`).
- **Observable Consequence**:
  The active tab appeared as a solid blue shape with invisible text.
- **Minimal Safe Fix**:
  Updated XML layouts to `android:textColor="@color/white"` on all active tabs/chips, and updated Kotlin click listeners (`ContactsActivity.kt`, `AlertsActivity.kt`, `RegionSelectionActivity.kt`, `AdminDashboardActivity.kt`) to set `chip.setTextColor(ContextCompat.getColor(this, R.color.white))` when selected.
- **Verification Status**: `FIXED AND TESTED`. Verified across all layouts.

---

### FINDING-UI-03: Bottom Navigation Bar Overlap in Admin Dashboard
- **Severity**: **Medium**
- **Location**: [`app/src/main/res/layout/activity_admin_dashboard.xml`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/res/layout/activity_admin_dashboard.xml)
- **Subsystem**: Layout View Hierarchy
- **Root Cause**:
  `adminDashboardScrollView` had no bottom padding, causing the floating `view_bottom_nav` bar (height 56dp + elevation) to physically cover the lowest emergency cards and the "Admin Logout" button.
- **Observable Consequence**:
  Citizens and field admins could not tap the bottom card actions or log out without accidental navigation taps.
- **Minimal Safe Fix**:
  Added `android:paddingBottom="84dp"` and `android:clipToPadding="false"` to `adminDashboardScrollView`.
- **Verification Status**: `FIXED AND TESTED`. Content scrolls completely clear of navigation controls.

---

### FINDING-MAP-01: Missing High-DPI Scaling on Offline Vector Maps
- **Severity**: **Medium**
- **Location**: [`app/src/main/kotlin/com/example/georescux/ui/common/OfflineMapSetup.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/ui/common/OfflineMapSetup.kt)
- **Subsystem**: OSMDroid / MapsForge Offline Map Engine
- **Root Cause**:
  `MapView` was initialized without enabling tile DPI scaling. On modern XXHDPI and XXXHDPI devices (400–560 dpi), raw 256x256 tiles rendered at microscopic physical scale with illegible road labels.
- **Observable Consequence**:
  Maps appeared stretched, blurry, or had unreadably tiny street names during evacuation navigation.
- **Minimal Safe Fix**:
  Added `mapView.isTilesScaledToDpi = true`, `mapView.minZoomLevel = 4.0`, and `mapView.maxZoomLevel = 20.0` in `OfflineMapSetup.kt`.
- **Verification Status**: `FIXED AND TESTED`. Verified in map initialization pathways.

---

### FINDING-PERF-01: Redundant Graph Re-parsing on Regional Evacuation Routing
- **Severity**: **Low**
- **Location**: [`app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt)
- **Subsystem**: Offline Routing / Memory Management
- **Root Cause**:
  Regional graphs are 18–33 MB in size. When querying routes, repeated JSON string parsing caused high CPU and garbage collector churn.
- **Remediation**:
  `RouteRepositoryImpl` implements synchronized in-memory memoization (`cachedGraph`), verifying version timestamps before triggering seed graph asset parsing.
- **Verification Status**: `VERIFIED WORKING`. Graph parsed once and held in memory.
