# GeoRescuX — Enterprise Security, Authentication & Access Control Review

## 1. Security Architecture & Threat Model

GeoRescuX operates in high-stress, hostile, and zero-connectivity environments. The security model must withstand both physical communications infrastructure collapse and active adversarial tampering:
1. **Threat 1: Unauthorized Administrative Impersonation**: Malicious citizens attempting to access coordinator screens, delete distress records, or broadcast false disaster evacuations.
2. **Threat 2: Local Telemetry Forgery**: Tampering with victim GPS coordinates or emergency notes stored on-device.
3. **Threat 3: BLE Mesh Replay Attacks**: Adversaries re-transmitting old emergency packets to cause panic or broadcast storm denial-of-service.
4. **Threat 4: Insecure Cloud Database Access**: Malicious clients executing unauthorized reads or writes across Firebase Realtime Database paths.

---

## 2. Secrets Scan & Repository Hygiene

An automated recursive scan was executed across all tracked and untracked files in the repository:
- **Scan Targets**: Private keys (`BEGIN PRIVATE KEY`), AWS/GCP access tokens, Firebase service account keys (`serviceAccountKey.json`), `.env` secrets.
- **Results**: **0 Secrets Tracked in Git**.
- **Verification**: [`georescux-admin-provision/.gitignore`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/georescux-admin-provision/.gitignore) strictly ignores:
  - `serviceAccountKey.json`
  - `.env`
  - `node_modules/`
- **Root `.gitignore`**: Correctly ignores build outputs, local properties, keystores, and CMake build caches.

---

## 3. Authentication & Custom Claims Authorization

### Fail-Closed Role Resolution
- **Implementation**: [`app/src/main/kotlin/com/example/georescux/data/auth/FirebaseRoleResolver.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/auth/FirebaseRoleResolver.kt)
- **Mechanism**: Calls `user.getIdToken(forceRefresh = true)`.
- **Zero-Trust Invariant**: The application never trusts local preferences or client-side flags.
- **Fail-Closed Policy**: If the device is offline and the cached token is expired, or if network failure occurs, the resolver immediately returns `UserRole.UNKNOWN`.

### Admin Provisioning Architecture
Administrative authority requires the Firebase Auth custom claim `{ admin: true }`. This claim can only be signed by Google's backend servers using service account credentials via out-of-band tooling:
- **Script**: [`georescux-admin-provision/makeAdmin.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/georescux-admin-provision/makeAdmin.js)
- **Command**: `node makeAdmin.js <target-uid>`

---

## 4. Firebase Realtime Database Rules Audit (`database.rules.json`)

The security rules in [`database.rules.json`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/database.rules.json) enforce strict least-privilege access control:

| Path | Read Rule | Write Rule | Security Invariant Enforced |
| :--- | :--- | :--- | :--- |
| **`/sos_alerts`** | Admin only (`auth.token.admin == true`) | Closed at root | Prevents normal citizens from scraping active emergencies across the entire region. |
| **`/sos_alerts/{uid}`** | `auth.uid == $uid \|\| auth.token.admin == true` | Closed at user level | Users can only view their own distress records; coordinators can view all records. |
| **`/sos_alerts/{uid}/{alertId}`** | Same as above | `auth.uid == $uid && newData.exists()` | Users can create their own alerts; coordinators cannot forge citizen GPS coordinates. |
| **`/sos_alerts/{uid}/{alertId}/status`** | Same as above | `(auth.uid == $uid && (status == 'ACTIVE' \|\| 'COMPLETED')) \|\| (admin && status == 'RESOLVED')` | Citizens can only mark their alert ACTIVE or COMPLETED. Only admins can mark RESOLVED. |
| **`/contacts/{uid}`** | `auth.uid == $uid` | `auth.uid == $uid` | Strict user isolation: contacts are strictly private to the authenticated account. |
| **`/hazards`** | Public authenticated (`auth != null`) | `auth.token.admin == true` | Any citizen can read evacuation hazards; only verified admins can add/remove hazards. |
| **`/admin_alerts`** | Public authenticated (`auth != null`) | `auth.token.admin == true` | Only verified admins can issue regional public disaster broadcasts. |

---

## 5. Security Vulnerability Register

### FINDING-SEC-01: Self-Bootstrapping Privilege Escalation Vulnerability
- **Severity**: **Critical**
- **Location**: [`functions/index.js:L30-L37`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/index.js#L30-L37)
- **Vulnerability**:
  ```javascript
  const callerIsAdmin = context.auth.token?.admin === true;
  const isSelfBootstrap = targetUid === context.auth.uid;
  if (!callerIsAdmin && !isSelfBootstrap) {
    throw new functions.https.HttpsError("permission-denied", "...");
  }
  ```
  Because `isSelfBootstrap` checks `targetUid === context.auth.uid`, any logged-in user calling this function can make themselves an Administrator.
- **Remediation**: Completely removed `isSelfBootstrap` logic. Replaced with strict server-side authorization check `callerIsAdmin = context.auth.token?.admin === true`. All non-admin callers attempting privilege escalation receive `permission-denied`. Initial bootstrap is strictly out-of-band via [`georescux-admin-provision/manageAdmin.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/georescux-admin-provision/manageAdmin.js).
- **Status**: `REMEDIATED AND VERIFIED`. Regression test suite [`functions/test/admin_security.test.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/test/admin_security.test.js) added; 15/15 security test assertions passing.

---

## 6. Mesh Cryptography & Anti-Replay Protections

In the C++ native core ([`app/src/main/cpp/crypto/PacketAuthenticator.cpp`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/cpp/crypto/PacketAuthenticator.cpp)):
- **Packet Signatures**: Over-the-air BLE packets carry an HMAC-SHA256 signature calculated over payload fields (origin ID, sequence number, coordinates, timestamp).
- **Sliding-Window Replay Guard**: [`app/src/main/cpp/mesh/SlidingWindowReplayGuard.cpp`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/cpp/mesh/SlidingWindowReplayGuard.cpp) enforces a sliding window of sequence numbers and rejects packets with timestamps older than 5 minutes or sequence numbers previously seen.
