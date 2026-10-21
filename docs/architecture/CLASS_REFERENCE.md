# GeoRescuX — Master Class Reference & Interface Index

## 1. Kotlin Android Production Classes

### Application & Dependency Injection
- **`GeoRescuXApplication`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt:L353`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt#L353)
  - *Responsibility*: Process root extending `Application`. Owns `AppContainer` and starts background services.
- **`AppContainer`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt:L57`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt#L57)
  - *Responsibility*: Manual dependency injection container providing singletons for repositories, use cases, and coordinators.

### Emergency & SOS Subsystem
- **`SosEmergency`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/domain/sos/SosEmergency.kt:L10`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/sos/SosEmergency.kt#L10)
  - *Responsibility*: Immutable domain data class representing an emergency event.
- **`SosRepository`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/domain/repository/SosRepository.kt:L11`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/repository/SosRepository.kt#L11)
  - *Responsibility*: Domain interface defining SOS lifecycle operations (`startEmergency`, `completeEmergency`, `updateActiveEmergencyLocation`).
- **`SosRepositoryImpl`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt:L13`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt#L13)
  - *Responsibility*: Local storage implementation enforcing single-active SOS rule.
- **`SyncingSosRepository`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/sync/SyncingSosRepository.kt:L20`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sync/SyncingSosRepository.kt#L20)
  - *Responsibility*: Decorator repository mirroring local changes to Firebase Realtime Database.
- **`SosRelayRepository`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/sos/SosRelayRepository.kt:L12`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRelayRepository.kt#L12)
  - *Responsibility*: Decorator repository broadcasting SOS alerts over Google Nearby mesh.
- **`SosBleBridgeRepository`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/sos/SosBleBridgeRepository.kt:L15`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosBleBridgeRepository.kt#L15)
  - *Responsibility*: Decorator repository broadcasting SOS alerts over raw BLE GATT frames.

### Offline Routing & Maps Subsystem
- **`RouteRepository`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/domain/repository/RouteRepository.kt:L10`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/repository/RouteRepository.kt#L10)
  - *Responsibility*: Interface for route calculation and road obstruction management.
- **`RouteRepositoryImpl`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt:L23`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/routing/RouteRepositoryImpl.kt#L23)
  - *Responsibility*: Local routing repository with in-memory memoization of regional graphs.
- **`AStarPathfinder`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/domain/routing/AStarPathfinder.kt:L21`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/routing/AStarPathfinder.kt#L21)
  - *Responsibility*: Pure Kotlin A* pathfinding algorithm over topological road graphs.
- **`SafeHavenFallback`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/domain/routing/SafeHavenFallback.kt:L8`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/routing/SafeHavenFallback.kt#L8)
  - *Responsibility*: Fallback router navigating citizens to nearest reachable safe shelter when requested destination is blocked.
- **`GeoRescueNativeRouting`**:
  - *Location*: [`app/src/main/kotlin/com/example/georescux/data/routing/GeoRescueNativeRouting.kt:L21`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/routing/GeoRescueNativeRouting.kt#L21)
  - *Responsibility*: JNI bridge to `libgeorescux_routing.so` with safe fallback to Kotlin.

---

## 2. C++20 / Qt 6.11.1 Prototype Classes

### Application State & Orchestration
- **`georescux::app::AppState`**:
  - *Location*: [`prototype_qt/src/app/AppState.h:L44`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/app/AppState.h#L44)
  - *Responsibility*: Central authoritative state machine exposing Q_PROPERTY bindings to QML views.
- **`georescux::domain::EmergencyService`**:
  - *Location*: [`prototype_qt/src/domain/EmergencyService.h:L19`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/EmergencyService.h#L19)
  - *Responsibility*: Service controller managing countdowns, activations, and SQLite writes.

### Interface Contracts & Storage
- **`georescux::domain::IEmergencyRepository`**:
  - *Location*: [`prototype_qt/src/domain/IEmergencyRepository.h:L16`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/IEmergencyRepository.h#L16)
  - *Responsibility*: Pure virtual contract for emergency persistence.
- **`georescux::storage::SqliteEmergencyRepository`**:
  - *Location*: [`prototype_qt/src/platform/storage/SqliteEmergencyRepository.h:L14`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/platform/storage/SqliteEmergencyRepository.h#L14)
  - *Responsibility*: SQLite-backed transactional emergency storage.
- **`georescux::domain::ILocationProvider`**:
  - *Location*: [`prototype_qt/src/domain/ILocationProvider.h:L13`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/ILocationProvider.h#L13)
  - *Responsibility*: Abstract hardware interface for location acquisition and permission status.
- **`georescux::domain::FakeLocationProvider`**:
  - *Location*: [`prototype_qt/src/domain/FakeLocationProvider.h:L16`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/FakeLocationProvider.h#L16)
  - *Responsibility*: Deterministic mock provider for automated unit testing.
- **`georescux::platform::qt::QtLocationProvider`**:
  - *Location*: [`prototype_qt/src/platform/qt/QtLocationProvider.h:L15`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/platform/qt/QtLocationProvider.h#L15)
  - *Responsibility*: Production GPS provider using Qt Location / QGeoPositionInfoSource.
