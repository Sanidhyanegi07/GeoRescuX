# GeoRescuX — Standalone C++20 Drogon Emergency Backend

This directory contains the standalone, high-performance C++20 Drogon REST backend for GeoRescuX. It operates independently from the Android Gradle build and provides low-latency ingestion, durable SQLite persistence, and cryptographic Firebase ID token verification.

## 1. Architecture

```
Android Client (GeoRescuX)
       │ (Optional HTTPS POST)
       ▼
[Drogon C++20 Web Framework]
  ├── /health (HealthController)
  ├── FirebaseAuthFilter (RS256 JWT Verification & Firebase Claims)
  │     ├── FirebaseAdminFilter (Enforces "admin: true" claim)
  │     ├── /api/v1/sos/events (SosEventController)
  │     ├── /api/v1/admin/sos/events (Admin access only)
  │     └── /api/v1/mesh/packets (BleMeshController)
  └── [SqliteStorage] ──> georescux_backend.db (Durable persistence)
```

## 2. API Endpoints

| Method | Endpoint | Auth | Purpose |
|---|---|---|---|
| `GET` | `/health` | Public | Service health check (`status: "UP"`, schema version) |
| `POST` | `/api/v1/sos/events` | Bearer Token | Ingest/update emergency SOS event (Idempotent by `eventId`) |
| `GET` | `/api/v1/sos/events/{id}` | Bearer Token | Retrieve single emergency SOS event |
| `GET` | `/api/v1/admin/sos/events` | Bearer Token (Admin) | Retrieve all emergency SOS events (Disaster Coordinator view) |
| `POST` | `/api/v1/mesh/packets` | Bearer Token | Ingest BLE mesh store-and-forward packet (Deduplicated by `packetId`) |
| `GET` | `/api/v1/mesh/packets/{id}` | Bearer Token | Retrieve stored BLE mesh packet |

## 3. Firebase Authentication & Security Invariants

1. **Cryptographic Verification:**
   - Tokens must be valid RS256 JWTs signed by Google Accounts for Firebase project `georescex`.
   - Rejects unsigned, forged, or alg-none tokens.
   - Enforces audience (`aud == georescex`), issuer (`iss == https://securetoken.google.com/georescex`), and expiration (`exp > now`).
2. **Role-Based Access Control:**
   - Admin routes (`/api/v1/admin/*`) require the Firebase Custom Claim `admin: true`.
   - Ordinary authenticated users receive `403 Forbidden`.
3. **Zero Secrets in Repository:**
   - No private keys, database passwords, or service account files are stored in this codebase.

## 4. Build Instructions

### Linux (Ubuntu 22.04 / 24.04)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libsqlite3-dev libjsoncpp-dev libssl-dev libuuid1 uuid-dev

# Install Drogon
git clone https://github.com/drogonframework/drogon.git
cd drogon
git submodule update --init
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) && sudo make install
cd ../..

# Build GeoRescuX Backend
cd backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure

# Start server
./georescux_backend
```

### Windows (vcpkg / MSVC)
```powershell
vcpkg install drogon:x64-windows sqlite3:x64-windows
cd backend
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="<vcpkg_root>/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release
.\Release\georescux_backend.exe
```
