# GeoRescuX — Known Limitations & Residual Risk Register

## 1. Scope & Transparency Statement

In mission-critical life-safety software, claiming an application is "100% production-ready" without full physical-device and field validation is hazardous. This document transparently records every known technical limitation, environmental constraint, and residual risk across the GeoRescuX repository.

---

## 2. Hardware & Radio Frequency (RF) Limitations

### LIMIT-HW-01: Dual-Device Bluetooth Low Energy Physical Validation
- **Constraint**: Bluetooth Low Energy (BLE) peripheral advertising and central scanning operate over 2.4 GHz ISM radio frequencies subject to physical attenuation, multipath interference, body shadowing, and hardware antenna diversity.
- **Current State**: Automated unit tests verify packet encoding, checksum calculation, and Bloom filter deduplication. CTest suites verify packet codecs.
- **Limitation**: Real multi-hop mesh forwarding across 3 or more physical devices in an active RF environment requires on-site testing with physical Android hardware. Single-workstation testing cannot validate radio range or antenna performance.
- **Mitigation Strategy**: The application implements dual mesh transport: standard BLE GATT broadcast (UUID `0x180D`) and Google Nearby Connections with Wi-Fi Direct fallback.

### LIMIT-HW-02: Live GNSS / GPS Satellite Constellation Acquisition
- **Constraint**: Physical GPS accuracy depends on satellite line-of-sight (GPS, GLONASS, Galileo, BeiDou), atmospheric delay, and urban canyon reflection.
- **Current State**: Unit tests verify coordinate caching, timestamping, null-protection, and dead-reckoning mathematics.
- **Limitation**: Hardware time-to-first-fix (TTFF), cold-start latency, and indoor multipath degradation cannot be verified without physical satellite reception.
- **Mitigation Strategy**: The location repository preserves the last known valid coordinate fix if GPS signal is temporarily lost during a tunnel transit or structure collapse (`val resolved = location ?: active.location`).

---

## 3. Operating System & Background Execution Constraints

### LIMIT-OS-01: Android 12+ (API 31–35) Foreground Service & Battery Optimization
- **Constraint**: Modern Android versions enforce aggressive background execution restrictions (Doze mode, App Standby buckets, and Phantom Process killer). Background BLE scanning without an active foreground service is throttled to 15-minute windows or disabled entirely by OEM power managers (MIUI, ColorOS, OneUI).
- **Current State**: GeoRescuX implements `GeoRescueBleForegroundService` with an ongoing notification channel.
- **Limitation**: If the user revokes `POST_NOTIFICATIONS` or enables extreme battery-saver mode, background mesh relay will be curtailed by the OS.
- **Mitigation Strategy**: The app actively requests foreground service permissions and battery optimization exemption during SOS activation.

---

## 4. Cloud & Security Infrastructure Boundaries

### LIMIT-SEC-01: Historical Self-Bootstrapping Logic Remediated & Verified
- **Resolution**: In [`functions/index.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/index.js#L30-L36), `isSelfBootstrap` was completely removed and replaced with strict server-side authorization (`callerIsAdmin = context.auth.token?.admin === true`).
- **Status**: Remediated and verified with [`functions/test/admin_security.test.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/functions/test/admin_security.test.js) (12/12 passing). Initial administrative bootstrap is strictly restricted to out-of-band provisioning via [`georescux-admin-provision/manageAdmin.js`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/georescux-admin-provision/manageAdmin.js). Cloud function is un-deployed pending live production environment setup.

### LIMIT-CLOUD-01: Offline-First Synchronous Mirroring Lag
- **Constraint**: When operating in a zero-connectivity disaster zone, local emergency updates are queued durably in `SyncStateStore`.
- **Limitation**: The administrative command portal (`AdminDashboardActivity`) cannot view SOS alerts until at least one mesh device encounters an active network uplink (cellular, satellite, or emergency Wi-Fi).
- **Mitigation Strategy**: The app acts as an epidemic store-and-forward "data mule": any nearby responder entering the disaster area will automatically absorb and carry pending distress alerts to the nearest uplink point.
