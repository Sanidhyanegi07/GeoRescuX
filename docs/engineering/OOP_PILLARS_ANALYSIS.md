# GeoRescuX — Object-Oriented Programming (OOP) Pillars Analysis

## 1. Executive Summary

Object-Oriented Programming (OOP) forms the architectural backbone of both the GeoRescuX Kotlin production Android application and the C++20 / Qt 6.11.1 native prototype. In mission-critical emergency response software, object-oriented design is not merely stylistic; it is vital for ensuring life-safety invariants:
- **Encapsulation** protects mutable emergency records and cryptographic keys against external corruption or invalid state transitions.
- **Inheritance** provides structured integration with platform lifecycles (Android `Application`/`AppCompatActivity` and Qt `QObject`/`QAbstractListModel`/`QQuickPaintedItem`).
- **Polymorphism** allows transparent substitution of storage engines, routing algorithms, and network transports at runtime.
- **Abstraction** isolates volatile external technologies (Firebase Realtime Database, BLE GATT, MapsForge, GPS hardware) behind stable domain contracts.

---

## 2. Comprehensive Analysis: The Four Pillars in Kotlin

### A. Encapsulation in Kotlin

#### 1. Concept in Simple Language
Encapsulation bundles data (state) and the methods that operate on that data into a single unit (class), while hiding internal details and private state from outside code. It guarantees that internal data can only be modified through controlled, validated operations.

#### 2. Exact Source Location
- **File**: [`app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt)
- **Class**: `SosRepositoryImpl`
- **Lines**: 13–48

#### 3. Code Evidence
```kotlin
class SosRepositoryImpl(private val store: LocalSosStore) : SosRepository {

    override fun getActiveEmergency(): SosEmergency? = store.loadActive()

    override fun startEmergency(id: String, startedAtMs: Long): SosEmergency? {
        // Safety rule: never allow two simultaneous emergencies.
        if (store.loadActive() != null) return null
        val emergency = SosEmergency(id = id, startedAtMs = startedAtMs)
        store.saveActive(emergency)
        return emergency
    }

    override fun completeEmergency(stoppedAtMs: Long): SosEmergency? {
        val active = store.loadActive() ?: return null
        val completed = active.copy(stoppedAtMs = stoppedAtMs)
        store.clearActive()
        store.addToHistory(completed)
        return completed
    }

    override fun updateActiveEmergencyLocation(location: SosLocation?, status: SosLocationStatus) {
        val active = store.loadActive() ?: return
        val resolvedLocation = location ?: active.location
        store.saveActive(active.copy(location = resolvedLocation, locationStatus = status))
    }

    override fun updateActiveEmergencyNote(note: String?) {
        val active = store.loadActive() ?: return
        val capNote = note?.trim()?.takeIf { it.isNotEmpty() && !it.equals("null", ignoreCase = true) }?.take(120)
        store.saveActive(active.copy(note = capNote))
    }
}
```

#### 4. Why This Satisfies Encapsulation
- The backing store `private val store: LocalSosStore` is completely private and inaccessible outside `SosRepositoryImpl`.
- Outside callers cannot arbitrarily overwrite emergency history or force two simultaneous emergencies.
- The life-critical invariant *"a citizen may only have at most ONE active emergency"* is strictly enforced in `startEmergency` (line 19: `if (store.loadActive() != null) return null`).
- Coordinate updates in `updateActiveEmergencyLocation` guarantee that a `null` GPS fix will not obliterate previously acquired valid coordinates (`val resolvedLocation = location ?: active.location`).
- Emergency notes are validated, trimmed, sanitized against `"null"`, and bounded to 120 characters before reaching the store.

#### 5. Runtime Usage
`SosRepositoryImpl` is instantiated once in `AppContainer` (`GeoRescuXApplication.kt`, line 92) and wrapped by decorator repositories before being injected into ViewModels and background workers.

---

### B. Inheritance in Kotlin

#### 1. Concept in Simple Language
Inheritance allows a child class to inherit fields and methods from a parent class or implement specified behavior from an interface, enabling code reuse and establishing an *"is-a"* relationship.

#### 2. Exact Source Location
- **File**: [`app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/GeoRescuXApplication.kt)
- **Class**: `GeoRescuXApplication` inheriting from Android framework `Application`
- **Lines**: 353–407

#### 3. Code Evidence
```kotlin
class GeoRescuXApplication : Application() {
    val appContainer: AppContainer by lazy { AppContainer(this) }
    private val appScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)

    override fun onCreate() {
        super.onCreate()
        appScope.launch {
            val coordinator = appContainer.syncRetryCoordinator
            coordinator.start()
            coordinator.retryPendingNow()
            // ...
        }
    }

    override fun onTerminate() {
        super.onTerminate()
        appContainer.syncRetryCoordinator.stop()
        appContainer.relayConnectionManager.stopMesh()
        runCatching { appContainer.bleManager.shutdown() }
    }
}
```

#### 4. Why This Satisfies Inheritance
- `GeoRescuXApplication` explicitly subclasses `android.app.Application` via `: Application()`.
- It overrides Android framework lifecycle hooks (`onCreate()` and `onTerminate()`), invoking `super.onCreate()` to preserve operating system initializations while injecting GeoRescuX's background worker initializations.
- This establishes an *"is-a"* relationship: `GeoRescuXApplication` is an Android `Application`, allowing the Android OS `ActivityManager` to instantiate it as the process root defined in `AndroidManifest.xml`.

#### 5. Distinction: Class Inheritance vs. Interface Implementation
In Kotlin, `: Application()` is strict class inheritance (extending an existing class implementation with state). In contrast, `: SosRepository` is interface implementation (agreeing to fulfill a method contract without inheriting code).

---

### C. Polymorphism in Kotlin

#### 1. Concept in Simple Language
Polymorphism allows different underlying classes to be accessed through the same unified interface. When a method is called on the interface, the specific implementation executed is determined dynamically at runtime based on the actual object instance.

#### 2. Exact Source Location
- **Files**:
  - [`app/src/main/kotlin/com/example/georescux/domain/repository/SosRepository.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/repository/SosRepository.kt) (Contract, lines 11–39)
  - [`app/src/main/kotlin/com/example/georescux/data/sos/SosBleBridgeRepository.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosBleBridgeRepository.kt) (Decorator, lines 15–54)
  - [`app/src/main/kotlin/com/example/georescux/data/sos/SosRelayRepository.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRelayRepository.kt) (Decorator, lines 12–50)
  - [`app/src/main/kotlin/com/example/georescux/data/sync/SyncingSosRepository.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sync/SyncingSosRepository.kt) (Decorator, lines 20–85)
  - [`app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/data/sos/SosRepositoryImpl.kt) (Terminal, lines 13–48)

#### 3. Code Evidence & Runtime Call Chain
In `AppContainer` (`GeoRescuXApplication.kt`, lines 92–264):
```kotlin
// 1. Terminal Store: Writes to SharedPreferences
val localSosRepository: SosRepository = SosRepositoryImpl(sosStore)

// 2. Cloud Mirror: Syncs to Firebase Realtime Database
val syncingSosRepository: SyncingSosRepository = SyncingSosRepository(
    local = localSosRepository,
    authRepository = authRepository,
    cloudDataSource = FirebaseSosCloudDataSource(),
    syncStateStore = syncStateStore
)

// 3. Nearby Mesh: Broadcasts over Google Nearby Connections
val sosRelayRepository: SosRepository = SosRelayRepository(
    delegate = syncingSosRepository,
    relayEngine = relayEngine,
    selfOriginId = selfOriginId
)

// 4. Raw BLE GATT: Broadcasts over 0x180D Bluetooth mesh
val sosBleBridgeRepository: SosRepository = SosBleBridgeRepository(
    delegate = sosRelayRepository,
    bridge = bleSosBridge
)

// Injected into ViewModels as generic interface:
val sosRepository: SosRepository = sosBleBridgeRepository
```

When `SosViewModel` calls `sosRepository.startEmergency("SOS_123", timestamp)`:
1. Polymorphic call dispatches to `SosBleBridgeRepository.startEmergency(...)` -> Starts BLE advertising.
2. Dispatches to `SosRelayRepository.startEmergency(...)` -> Emits event over Nearby mesh.
3. Dispatches to `SyncingSosRepository.startEmergency(...)` -> Marks pending sync in `SyncStateStore`.
4. Dispatches to `SosRepositoryImpl.startEmergency(...)` -> Commits atomically to local storage.

#### 4. Why This Improves GeoRescuX
The presentation layer (`SosViewModel`, `SosActivity`) has zero awareness of BLE GATT, Google Nearby, or Firebase. It interacts strictly with `SosRepository`. Different network layers can be added or removed without changing a single line of UI code.

---

### D. Abstraction in Kotlin

#### 1. Concept in Simple Language
Abstraction hides complex background implementation details and exposes only the essential operations to the client.

#### 2. Exact Source Location
- **File**: [`app/src/main/kotlin/com/example/georescux/domain/repository/RouteRepository.kt`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/app/src/main/kotlin/com/example/georescux/domain/repository/RouteRepository.kt)
- **Interface**: `RouteRepository`
- **Lines**: 10–25

#### 3. Code Evidence
```kotlin
interface RouteRepository {
    fun getGraph(): RouteGraph
    fun saveGraph(graph: RouteGraph)
    fun findRoute(startNodeId: String, destinationNodeId: String): EvacuationRoute?
    fun setRoadBlocked(fromNodeId: String, toNodeId: String, blocked: Boolean)
    fun setHazard(hazard: RoadHazard)
    fun removeHazard(hazardId: String)
}
```

#### 4. Why This Satisfies Abstraction
The `RouteRepository` interface completely abstracts away:
- Whether the graph was parsed from a 33 MB OpenStreetMap PBF binary file or loaded from disk JSON.
- Whether pathfinding is executed in C++ via JNI (`GeoRescueNativeRouting`) or Kotlin (`SafeHavenFallback`).
- Whether road blockages are stored in memory or committed to SharedPreferences.
The caller (`RouteActivity`) simply calls `findRoute("NODE_A", "NODE_B")` and receives an `EvacuationRoute`.

---

## 3. Comprehensive Analysis: The Four Pillars in C++ / Qt

### A. Encapsulation in C++

#### 1. Concept in Simple Language
Encapsulation in C++ restricts direct access to an object's member variables using `private` or `protected` access specifiers, providing access only through member functions (getters, setters, action methods) that validate data and maintain class invariants.

#### 2. Exact Source Location
- **File**: [`prototype_qt/src/domain/EmergencyService.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/EmergencyService.h)
- **Class**: `georescux::domain::EmergencyService`
- **Lines**: 19–100

#### 3. Code Evidence
```cpp
class EmergencyService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasActiveEmergency READ hasActiveEmergency NOTIFY activeEmergencyChanged)
    Q_PROPERTY(QString activeEmergencyId READ activeEmergencyId NOTIFY activeEmergencyChanged)

public:
    explicit EmergencyService(std::shared_ptr<IEmergencyRepository> repository, QObject *parent = nullptr);
    [[nodiscard]] bool hasActiveEmergency() const noexcept { return m_activeRecord.has_value(); }

    Q_INVOKABLE bool activateEmergency(const QString &userId, const QString &note = QString());
    Q_INVOKABLE bool completeEmergency(const QString &userId, const QString &emergencyId);
    Q_INVOKABLE bool updateActiveNote(const QString &userId, const QString &emergencyId, const QString &note);

private:
    void setActiveRecord(const std::optional<EmergencyRecord> &record);

    std::shared_ptr<IEmergencyRepository> m_repository;
    std::optional<EmergencyRecord> m_activeRecord{std::nullopt};
    QString m_lastError;
};
```

In `EmergencyService.cpp` (lines 35–48):
```cpp
bool EmergencyService::activateEmergency(const QString &userId, const QString &note) {
    if (m_activeRecord.has_value()) {
        m_lastError = QStringLiteral("An emergency is already active.");
        emit errorOccurred(m_lastError);
        return false;
    }
    // Transactional persistence executed before memory state mutation
    auto result = m_repository->activateEmergency(userId, note);
    if (!result.isSuccess()) {
        m_lastError = result.errorMessage();
        emit errorOccurred(m_lastError);
        return false;
    }
    setActiveRecord(result.value());
    return true;
}
```

#### 4. Why This Satisfies Encapsulation
- `m_repository`, `m_activeRecord`, and `m_lastError` are strictly `private`.
- The member function `activateEmergency` enforces that an active emergency cannot be overwritten if one is already in progress (`if (m_activeRecord.has_value()) return false`).
- State mutation occurs only after the underlying SQLite repository confirms durable write success (`if (!result.isSuccess()) return false`).

---

### B. Inheritance in C++

#### 1. Concept in Simple Language
In C++, a derived class inherits member functions and attributes from a base class (`class Derived : public Base`). It allows polymorphic usage and code reuse.

#### 2. Exact Source Location
- **File**: [`prototype_qt/src/platform/storage/SqliteEmergencyRepository.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/platform/storage/SqliteEmergencyRepository.h)
- **Class**: `SqliteEmergencyRepository` inheriting from pure virtual `IEmergencyRepository`
- **Lines**: 14–45

#### 3. Code Evidence
```cpp
class SqliteEmergencyRepository : public IEmergencyRepository {
public:
    explicit SqliteEmergencyRepository(std::shared_ptr<LocalDatabase> database);
    ~SqliteEmergencyRepository() override = default;

    EmergencyResult<std::optional<EmergencyRecord>> getActiveEmergency(const QString &userId) override;
    EmergencyResult<EmergencyRecord> activateEmergency(const QString &userId, const QString &note) override;
    EmergencyResult<EmergencyRecord> updateLocation(
        const QString &userId, const QString &emergencyId,
        const EmergencyLocation &location, EmergencyLocationStatus status
    ) override;
    EmergencyResult<EmergencyRecord> updateNote(const QString &userId, const QString &emergencyId, const QString &note) override;
    EmergencyResult<EmergencyRecord> completeEmergency(const QString &userId, const QString &emergencyId) override;
    EmergencyResult<std::vector<EmergencyRecord>> getEmergencyHistory(const QString &userId, int limit) override;

private:
    std::shared_ptr<LocalDatabase> m_db;
};
```

#### 4. Why This Satisfies Inheritance
- `SqliteEmergencyRepository` publicly inherits from `IEmergencyRepository` (`: public IEmergencyRepository`).
- It implements all pure virtual functions (`= 0`) marked in the interface, using the C++11 `override` specifier to ensure compiler-enforced signature verification.
- It inherits the virtual destructor `virtual ~IEmergencyRepository() = default`, ensuring that deleting an instance through a base pointer (`IEmergencyRepository*`) correctly invokes derived class destructors without memory leaks.

---

### C. Polymorphism in C++

#### 1. Concept in Simple Language
Polymorphism in C++ occurs when a call to a virtual member function is resolved at runtime based on the dynamic type of the object pointed to (via the Virtual Method Table or *vtable*).

#### 2. Exact Source Location
- **Files**:
  - [`prototype_qt/src/domain/ILocationProvider.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/ILocationProvider.h) (Interface, lines 13–28)
  - [`prototype_qt/src/domain/FakeLocationProvider.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/FakeLocationProvider.h) (Mock implementation, lines 16–85)
  - [`prototype_qt/src/platform/qt/QtLocationProvider.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/platform/qt/QtLocationProvider.h) (Hardware implementation, lines 15–40)
  - [`prototype_qt/src/domain/LocationService.cpp`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/LocationService.cpp) (Caller, lines 20–60)

#### 3. Code Evidence & Runtime Dispatch
In `ILocationProvider.h`:
```cpp
class ILocationProvider {
public:
    virtual ~ILocationProvider() = default;
    [[nodiscard]] virtual bool isLocationServicesEnabled() const = 0;
    virtual void startUpdates(bool highFrequencyEmergency = false) = 0;
    virtual void stopUpdates() = 0;
    // ...
};
```

In `LocationService.cpp`:
```cpp
void LocationService::startTracking(bool highFrequency) {
    if (!m_provider) return;
    // Polymorphic dynamic dispatch via vtable:
    m_provider->startUpdates(highFrequency);
}
```

#### 4. How Polymorphism Operates at Runtime
1. In production on an Android phone, `AppState` injects an instance of `QtLocationProvider` (which communicates with hardware GPS via `QGeoPositionInfoSource`).
2. In the automated test suite [`prototype_qt/tests/tst_location.cpp`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/tests/tst_location.cpp), `AppState` is constructed with `std::make_shared<FakeLocationProvider>()`.
3. `LocationService` calls `m_provider->startUpdates(true)`. The CPU dereferences `m_provider->_vptr` to find the address of `startUpdates`.
4. In production, it activates satellite GPS hardware. In the unit test, it begins emitting predetermined waypoint coordinates along an evacuation path. No conditional branching (`if/else`) is required in `LocationService`.

---

### D. Abstraction in C++

#### 1. Concept in Simple Language
Abstraction defines an interface without exposing the implementation, decoupling the consumer from underlying hardware, network, or database systems.

#### 2. Exact Source Location
- **File**: [`prototype_qt/src/domain/IEmergencyRepository.h`](file:///c:/Users/sanid/Documents/GitHub/GeoRescuX/prototype_qt/src/domain/IEmergencyRepository.h)
- **Class**: `georescux::domain::IEmergencyRepository`
- **Lines**: 16–73

#### 3. Code Evidence
```cpp
class IEmergencyRepository {
public:
    virtual ~IEmergencyRepository() = default;

    virtual EmergencyResult<std::optional<EmergencyRecord>> getActiveEmergency(const QString &userId) = 0;
    virtual EmergencyResult<EmergencyRecord> activateEmergency(const QString &userId, const QString &note = QString()) = 0;
    virtual EmergencyResult<EmergencyRecord> updateLocation(
        const QString &userId, const QString &emergencyId,
        const EmergencyLocation &location, EmergencyLocationStatus status
    ) = 0;
    virtual EmergencyResult<EmergencyRecord> completeEmergency(const QString &userId, const QString &emergencyId) = 0;
    virtual EmergencyResult<std::vector<EmergencyRecord>> getEmergencyHistory(const QString &userId, int limit = 20) = 0;
};
```

#### 4. Why This Satisfies Abstraction
`IEmergencyRepository` contains zero data members, zero SQL strings, and zero file I/O operations. It consists purely of pure virtual methods returning `EmergencyResult<T>`.
The consumer (`EmergencyService`) knows only that it can request or complete emergencies. It has no knowledge of SQLite table schemas, transactional locks, or SQL column indexes.

---

## 4. Master OOP Analysis Reference Tables

### Kotlin Production Application Matrix

| Pillar | Exact File Path | Class / Interface | Methods / Line Numbers | Concrete Usage | Verified Code Evidence |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Encapsulation** | `data/sos/SosRepositoryImpl.kt` | `SosRepositoryImpl` | `startEmergency()`, `completeEmergency()`, L13–48 | Protects active emergency state and enforces single-active SOS rule. | `private val store: LocalSosStore`, `if (store.loadActive() != null) return null` |
| **Inheritance** | `GeoRescuXApplication.kt` | `GeoRescuXApplication` | `onCreate()`, `onTerminate()`, L353–407 | Extends Android framework process lifecycle for background DI setup. | `class GeoRescuXApplication : Application()`, `super.onCreate()` |
| **Polymorphism** | `data/sos/SosRelayRepository.kt` | `SosRelayRepository` | `startEmergency()`, L12–50 | Delegates through `SosRepository` interface to Google Nearby mesh transport. | Implements `SosRepository`; runtime substitution via decorator pattern in `AppContainer`. |
| **Abstraction** | `domain/repository/RouteRepository.kt` | `RouteRepository` | `findRoute()`, `setRoadBlocked()`, L10–25 | Exposes clean evacuation routing contract; hides C++ JNI and OSM graph decoding. | `interface RouteRepository`, implemented by `RouteRepositoryImpl`. |

---

### C++20 / Qt Prototype Matrix

| Pillar | Exact File Path | Class / Interface | Methods / Line Numbers | Concrete Usage | Verified Code Evidence |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Encapsulation** | `prototype_qt/src/domain/EmergencyService.h` | `EmergencyService` | `activateEmergency()`, `m_activeRecord`, L31–100 | Enforces user-isolated emergency state and validates persistence before UI notification. | `private: std::shared_ptr<IEmergencyRepository> m_repository; std::optional<EmergencyRecord> m_activeRecord;` |
| **Inheritance** | `prototype_qt/src/platform/storage/SqliteEmergencyRepository.h` | `SqliteEmergencyRepository` | `: public IEmergencyRepository`, L14–45 | Inherits interface contract and overrides pure virtual database operations. | `class SqliteEmergencyRepository : public IEmergencyRepository`, `override` specifier on all methods. |
| **Polymorphism** | `prototype_qt/src/domain/ILocationProvider.h` | `ILocationProvider` | `startUpdates()`, `stopUpdates()`, L13–28 | Dispatches dynamically to either `QtLocationProvider` (GPS) or `FakeLocationProvider` (simulated). | Pure virtual methods (`= 0`); runtime dispatch via `_vptr` in `LocationService::startTracking`. |
| **Abstraction** | `prototype_qt/src/domain/IEmergencyRepository.h` | `IEmergencyRepository` | `getActiveEmergency()`, `activateEmergency()`, L16–73 | Completely decouples domain orchestration from SQLite queries and connection handling. | Pure abstract base class (`virtual ~IEmergencyRepository() = default; virtual ... = 0;`). |

---

## 5. Architectural Relationships & UML Diagram

```
+-----------------------------------------------------------------------------------+
|                            Class Relationship Diagram                             |
+-----------------------------------------------------------------------------------+

     <<interface>>                                       <<abstract>>
   SosRepository (Kotlin)                          IEmergencyRepository (C++)
         ^                                                    ^
         |                                                    |
         +------------------+                                 +-----------------------+
         | (implements)     | (implements)                    | (implements)          | (implements)
         |                  |                                 |                       |
  SosRepositoryImpl    SyncingSosRepository        SqliteEmergencyRepository     MockEmergencyRepo
  (Local Storage)      (Firebase Sync)             (SQLite on Device)            (In-Memory Unit Test)
         ^                  ^
         | (decorates)      | (decorates)
  SosRelayRepository   SosBleBridgeRepository
  (Google Nearby)      (Raw BLE 0x180D)
```

---

## 6. OOP Trade-Offs & Composition vs. Inheritance

### Why Composition is Preferred for Repositories
In `GeoRescuXApplication.kt`, rather than creating deep inheritance hierarchies (e.g. `SosRepository` -> `SyncingSosRepository` -> `BleSosRepository` -> `NearbySosRepository`), GeoRescuX utilizes **Decorator Composition**:
- Each repository implements `SosRepository` and accepts another `SosRepository` as a `delegate`.
- *Benefit*: Features can be stacked dynamically without fragile base class problems. If BLE is disabled in settings, the application simply omits `SosBleBridgeRepository` from the chain without modifying `SosRepositoryImpl`.
