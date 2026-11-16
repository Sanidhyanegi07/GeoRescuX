# GeoRescuX — TCS-307 PBL Viva Voce Question Bank & Definitive Answer Guide

**Institution:** Graphic Era Hill University (GEHU), Bhimtal Campus  
**Department:** Computer Science and Engineering  
**Subject:** TCS-307 (Object Oriented Programming with C++)  
**Evaluation:** PBL Progress Monitoring Form & End-Semester Viva Voce  
**Project Title:** GeoRescuX — Tactical Offline-First Disaster Evacuation & Mesh Communication System  

---

## Table of Contents
1. [Category 1: Project Overview & Real-World Motivation](#category-1-project-overview--real-world-motivation)
2. [Category 2: C++ Core Concepts & General OOP Theory](#category-2-c-core-concepts--general-oop-theory)
3. [Category 3: Classes, Objects & Encapsulation in GeoRescuX](#category-3-classes-objects--encapsulation-in-georescux)
4. [Category 4: Constructors, Destructors & RAII (Rule of 5)](#category-4-constructors-destructors--raii-rule-of-5)
5. [Category 5: The Inheritance Question & Architectural Defense](#category-5-the-inheritance-question--architectural-defense)
6. [Category 6: Function & Operator Overloading](#category-6-function--operator-overloading)
7. [Category 7: Standard Template Library (STL) Containers & Algorithms](#category-7-standard-template-library-stl-containers--algorithms)
8. [Category 8: Graph Routing & A* Pathfinding Mechanics](#category-8-graph-routing--a-pathfinding-mechanics)
9. [Category 9: Spatial Indexing (KD-Trees, R-Trees & Geodesic Math)](#category-9-spatial-indexing-kd-trees-r-trees--geodesic-math)
10. [Category 10: Mesh Networking, Bloom Filters & Cryptography](#category-10-mesh-networking-bloom-filters--cryptography)
11. [Category 11: JNI Bridge & Kotlin-to-C++ Dual Layer Architecture](#category-11-jni-bridge--kotlin-to-c-dual-layer-architecture)
12. [Category 12: Mobile Performance, Memory & Cache Optimization](#category-12-mobile-performance-memory--cache-optimization)
13. [Category 13: Offline Mapping & Emergency Engineering](#category-13-offline-mapping--emergency-engineering)

---

## Category 1: Project Overview & Real-World Motivation

### Q1. What is the core problem that GeoRescuX solves?
**Answer:**  
In major natural disasters—such as the 2013 Kedarnath cloudburst and 2023 Himachal flash floods—cellular towers, fiber optic cables, and electrical grids collapse within minutes. Standard navigation applications like Google Maps become completely useless because they rely on cloud servers and internet connectivity. GeoRescuX provides **genuine, zero-internet disaster evacuation routing** and **peer-to-peer Bluetooth Low Energy (BLE) emergency mesh communication**, enabling civilians and first responders to navigate to safe havens and transmit SOS distress signals even when the entire telecommunications infrastructure is down.

### Q2. Why did you choose Uttarakhand, Himachal Pradesh, Haryana, and Uttar Pradesh?
**Answer:**  
These four states represent a continuous, interconnected disaster-vulnerable geographical corridor in North India:
1. **Uttarakhand & Himachal Pradesh:** High-altitude Himalayan seismic zones prone to cloudbursts, landslides, and flash floods.
2. **Haryana & Uttar Pradesh:** Downstream river basins and plains prone to massive monsoon flooding (Yamuna and Ganges basins) that absorb evacuees and coordinate relief logistics.

### Q3. How does GeoRescuX divide responsibilities between Kotlin and C++?
**Answer:**  
GeoRescuX uses a **dual-layer architecture**:
- **Kotlin / Android Layer:** Handles the Android lifecycle, Material 3 user interface, hardware sensors (GPS, Camera, Bluetooth radio), and cloud synchronization (Firebase).
- **C++ Native Layer (via JNI):** Executes compute-intensive, memory-critical algorithms—including A* evacuation pathfinding, KD-Tree nearest-neighbor queries, elevation impedance calculation, and rolling Bloom filter packet deduplication—completely free from Java Virtual Machine (JVM) garbage collection pauses.

---

## Category 2: C++ Core Concepts & General OOP Theory

### Q4. What are the four fundamental pillars of Object-Oriented Programming, and how does C++ implement them?
**Answer:**  
1. **Encapsulation:** Binding data and methods into classes with access specifiers (`private`, `public`, `protected`).
2. **Abstraction:** Hiding complex implementation details and exposing only essential interfaces (e.g., exposing `findRoute()` while hiding internal graph relaxation).
3. **Inheritance:** Deriving new classes from existing base classes to promote code reuse and establish "is-a" relationships (supported via single, multiple, multilevel, and hierarchical inheritance).
4. **Polymorphism:** The ability of an entity to take multiple forms—implemented via function/operator overloading at compile time (static) and virtual functions/vtable lookups at runtime (dynamic).

### Q5. What is the difference between a `struct` and a `class` in C++?
**Answer:**  
In C++, the only difference is the **default access specifier**:
- In a `struct`, members and base classes are `public` by default.
- In a `class`, members and base classes are `private` by default.
In GeoRescuX, `struct` is used for plain-old-data (POD) types like `RouteNode`, `RouteEdge`, and `SpatialPoint`, while `class` is used for stateful, encapsulated managers like `RouteGraph`, `BloomFilterDeduplicator`, and `KDTree`.

### Q6. What is the difference between shallow copy and deep copy?
**Answer:**  
- **Shallow Copy:** Copies member variable values as-is. If an object contains pointers to dynamically allocated memory, only the memory address is copied, causing two objects to point to the same memory. When one is destroyed, the other is left with a dangling pointer (double-free crash).
- **Deep Copy:** Allocates new memory on the heap and duplicates the actual data pointed to by the pointers.
In GeoRescuX, we avoid accidental expensive deep copies in spatial structures by explicitly **deleting the copy constructor** (`KDTree(const KDTree&) = delete;`) and enforcing move semantics.

---

## Category 3: Classes, Objects & Encapsulation in GeoRescuX

### Q7. Can you point to a specific class in your C++ codebase and explain its members?
**Answer:**  
In `app/src/main/cpp/routing/RouteGraph.h`:
- **Class:** `RouteGraph`
- **Public Data Members:** `nodes` (`std::vector<RouteNode>`), `edges` (`std::vector<RouteEdge>`), `hazards` (`std::vector<RoadHazard>`).
- **Private Data Members:** `nodesById` (`std::unordered_map<std::string, RouteNode>`), `adjacency` (`std::unordered_map<std::string, std::vector<std::pair<RouteEdge, RouteNode>>>`).
- **Public Member Functions:** `node()`, `neighbors()`, `edgeBetween()`, `rebuildAdjacency()`.
- **Private Member Function:** `buildIndexAndAdjacency()`.

### Q8. Why is `adjacency` kept private in `RouteGraph`?
**Answer:**  
To preserve the **graph invariant**. The `adjacency` map must remain mathematically synchronized with the list of `edges`. If external code could insert or remove items directly in `adjacency`, the graph could become unidirectional or contain dangling pointers to deleted nodes. By making it private, the adjacency list can only be constructed and modified via `buildIndexAndAdjacency()` and `rebuildAdjacency()`.

---

## Category 4: Constructors, Destructors & RAII (Rule of 5)

### Q9. What is a Member Initializer List and why did you use it?
**Answer:**  
A Member Initializer List initializes member variables before the body of the constructor executes.  
In `app/src/main/cpp/routing/RouteGraph.cpp` (Lines 7–11):
```cpp
RouteGraph::RouteGraph(std::vector<RouteNode> n, std::vector<RouteEdge> e,
                       std::vector<RoadHazard> h, int ver)
    : nodes(std::move(n)), edges(std::move(e)), hazards(std::move(h)), version(ver) {
    buildIndexAndAdjacency();
}
```
If we assigned members inside the curly braces `{ ... }`, the compiler would first call default constructors for `nodes`, `edges`, and `hazards`, and then call assignment operators, resulting in redundant memory allocations. Using the initializer list with `std::move` transfers vector buffers directly in $O(1)$ time without copying.

### Q10. What is RAII, and where is it demonstrated in your native code?
**Answer:**  
**RAII (Resource Acquisition Is Initialization)** is a C++ idiom where resource lifecycle is bound to object lifetime. When an object is created on the stack, its constructor acquires resources; when it goes out of scope, its destructor automatically frees them.
In GeoRescuX, `KDTree` uses `std::unique_ptr<KDNode>`. When a `KDTree` object goes out of scope or is destroyed, `std::unique_ptr` automatically frees every dynamically allocated node in the tree recursively, eliminating memory leaks without requiring manual `delete` statements.

### Q11. Explain what the "Rule of 5" is and show how it is used in `KDTree.h`.
**Answer:**  
The Rule of 5 states that if a class customizes any one of: Destructor, Copy Constructor, Copy Assignment, Move Constructor, or Move Assignment, it should explicitly declare all five.
In `app/src/main/cpp/spatial/KDTree.h` (Lines 37–45):
```cpp
KDTree() = default;
~KDTree() = default;
KDTree(KDTree&&) noexcept = default;
KDTree& operator=(KDTree&&) noexcept = default;
KDTree(const KDTree&) = delete;
KDTree& operator=(const KDTree&) = delete;
```
Here, copying is explicitly **deleted** (`= delete`) because duplicating a 50,000-node spatial tree causes severe memory churn on mobile devices. Move operations are explicitly defaulted (`= default noexcept`), allowing fast, zero-copy pointer transfers.

---

## Category 5: The Inheritance Question & Architectural Defense

### Q12. (CRITICAL) "Why are there no inheritance or virtual functions in `app/src/main/cpp/`? Is your project not truly object-oriented?"
**Answer:**  
*"Our native routing engine deliberately avoids virtual inheritance in favor of **Data-Oriented Design (DOD)** for mobile CPU performance.  
1. **Cache Locality:** Mobile ARM CPUs (like Cortex-A55) have strict L1/L2 cache budgets. Virtual functions require a Virtual Method Table (`vtable`) pointer (`_vptr`) inside every object, adding an extra pointer dereference for every method call and causing CPU instruction cache misses.
2. **Compiler Inlining:** In our A* pathfinding inner loop, the compiler evaluates tens of thousands of road segments. With concrete, non-virtual classes, the Clang C++ compiler inlines heuristic and cost calculations directly into CPU registers. Virtual functions cannot be inlined at compile time because their target is resolved at runtime.
3. **Where Inheritance IS used:** In our architecture and testing subsystem (`prototype_qt/src/domain/`), we fully utilize pure virtual interfaces (`IEmergencyRepository`, `ILocationProvider`) and class inheritance (`FakeLocationProvider : public ILocationProvider`) to achieve Dependency Inversion and mock GPS hardware during unit testing."*

### Q13. Can you show where an Abstract Base Class and Virtual Destructor exist in your project?
**Answer:**  
In `prototype_qt/src/domain/IEmergencyRepository.h`:
```cpp
class IEmergencyRepository {
public:
    virtual ~IEmergencyRepository() = default; // Virtual Destructor
    virtual EmergencyResult<std::optional<EmergencyRecord>> getActiveEmergency(const QString &userId) = 0; // Pure Virtual Function
    virtual EmergencyResult<EmergencyRecord> activateEmergency(...) = 0;
};
```
And its concrete derived class `SqliteEmergencyRepository` implements these pure virtual methods. A virtual destructor ensures that when a derived class object is deleted through a base class pointer, the derived destructor is invoked first, preventing memory leaks.

---

## Category 6: Function & Operator Overloading

### Q14. What is the difference between compile-time and runtime polymorphism?
**Answer:**  
- **Compile-Time (Static) Polymorphism:** Resolved by the compiler before program execution. Implemented via **Function Overloading**, **Operator Overloading**, and **Templates**. It has zero runtime performance overhead.
- **Runtime (Dynamic) Polymorphism:** Resolved during program execution using **Virtual Functions** and **vtable pointers**. It incurs a small indirection penalty but allows dynamic swapping of behavior.

### Q15. Where have you used Function Overloading in GeoRescuX?
**Answer:**  
In `app/src/main/cpp/mesh/BloomFilterDeduplicator.h`:
```cpp
// Overload 1: Binary numeric identification for low-overhead BLE mesh packets
bool checkAndMark(uint32_t senderHash, uint16_t sequenceNumber);

// Overload 2: String-based convenience overload for UUID/incident packet tokens
bool checkAndMark(const std::string& packetIdentifier);
```
Both methods share the name `checkAndMark`, but the compiler distinguishes them based on parameter signature.

### Q16. Why did you overload `operator>` in `AStarPathfinder.cpp`?
**Answer:**  
In `app/src/main/cpp/routing/AStarPathfinder.cpp` (Lines 26–31):
```cpp
bool operator>(const SearchNode& other) const {
    if (std::abs(estimatedTotalCost - other.estimatedTotalCost) > 1e-9) {
        return estimatedTotalCost > other.estimatedTotalCost;
    }
    return nodeId > other.nodeId;
}
```
The C++ Standard Library `std::priority_queue` is a **Max-Heap** by default (it pops the largest element using `operator<`). For the A* algorithm, we require a **Min-Heap** to always pop the node with the lowest estimated total cost ($f(n)$). By overloading `operator>`, passing `std::greater<SearchNode>` to `std::priority_queue` reverses the comparison, transforming it into an efficient binary min-heap.

---

## Category 7: Standard Template Library (STL) Containers & Algorithms

### Q17. Why use `std::unordered_map` instead of `std::map` for the routing graph?
**Answer:**  
- `std::map` is implemented as a Red-Black self-balancing Binary Search Tree, offering $O(\log N)$ search, insertion, and deletion time.
- `std::unordered_map` is implemented as a Hash Table, providing average $O(1)$ constant time lookup.
In emergency road routing with 50,000 road nodes, looking up neighbor nodes in $O(1)$ time is significantly faster than traversing a tree in $O(\log N)$ time, preventing pathfinding latency.

### Q18. What is `std::optional` and why is it used instead of returning `nullptr`?
**Answer:**  
Introduced in C++17, `std::optional<T>` represents a value that may or may not exist without allocating heap memory or using null pointers.  
In `AStarPathfinder::findRoute`:
If an evacuation route to a safe haven is blocked by landslides, returning `std::nullopt` forces the caller to explicitly check whether a route was found (`if (route.has_value())`) before accessing it. This completely eliminates Null Pointer Exceptions (`SIGSEGV`).

---

## Category 8: Graph Routing & A* Pathfinding Mechanics

### Q19. How does the A* algorithm work in GeoRescuX?
**Answer:**  
A* finds the shortest path by evaluating nodes using the evaluation function:
$$f(n) = g(n) + h(n)$$
- $g(n)$: Exact travel cost accumulated from the start node to current node $n$.
- $h(n)$: Heuristic estimate of the remaining travel cost from node $n$ to the destination safe haven.
GeoRescuX maintains an open set min-heap (`std::priority_queue`) of candidate nodes, iteratively expanding the node with the minimum $f(n)$ until the destination is reached.

### Q20. What heuristic function do you use, and is it admissible?
**Answer:**  
We use the **Haversine Geodesic Distance Formula** on the Earth's sphere ($R = 6371\text{ km}$):
$$d = 2R \arcsin\left(\sqrt{\sin^2\left(\frac{\Delta \phi}{2}\right) + \cos(\phi_1)\cos(\phi_2)\sin^2\left(\frac{\Delta \lambda}{2}\right)}\right)$$
- **Admissibility:** A heuristic is admissible if it never overestimates the true cost to reach the goal ($h(n) \le h^*(n)$). Because the straight-line Haversine distance is the shortest possible path between two geographic coordinates on Earth ("as the crow flies"), the real road distance will always be greater than or equal to it. Therefore, our heuristic is **provably admissible**, guaranteeing that A* always returns the optimal path.
- **Consistency:** It satisfies the triangle inequality $h(A) \le c(A, B) + h(B)$, meaning no visited node needs to be re-evaluated.

### Q21. How do active disaster hazards (landslides, floods) modify the graph?
**Answer:**  
When an emergency hazard is reported (via BLE or offline sensor), `RouteCostPolicy::edgeCostMeters()` evaluates the affected road edge:
1. If the road is completely blocked (e.g., massive landslide), the edge cost returns `std::nullopt`, pruning that branch from the search tree.
2. If the hazard causes partial obstruction (e.g., waterlogging), an impedance penalty multiplier is added, causing A* to automatically route evacuees along safer alternative bypasses.

---

## Category 9: Spatial Indexing (KD-Trees, R-Trees & Geodesic Math)

### Q22. What is a KD-Tree and why is it needed in GeoRescuX?
**Answer:**  
A **2D KD-Tree (k-dimensional tree)** is a space-partitioning binary search tree for organizing points in a 2D coordinate space (Latitude and Longitude).
When a user opens the app during a disaster, their GPS coordinates may not lie exactly on a road intersection. GeoRescuX uses `KDTree::findNearestNeighbor()` to snap the user's GPS coordinates to the closest road network node in $O(\log N)$ time instead of scanning every node in $O(N)$ time.

### Q23. How does the KD-Tree alternate axes during construction?
**Answer:**  
In `app/src/main/cpp/spatial/KDTree.cpp`:
- At depth 0 (root), points are partitioned along the **Latitude axis** ($x$-axis) using the median point.
- At depth 1, points are partitioned along the **Longitude axis** ($y$-axis).
- The axis alternates at each level: `axis = depth % 2`. This creates a balanced 2D spatial hierarchy.

---

## Category 10: Mesh Networking, Bloom Filters & Cryptography

### Q24. How does the BLE mesh work without cell towers or Wi-Fi?
**Answer:**  
GeoRescuX implements an **Epidemic Gossip Routing Protocol** over Bluetooth Low Energy (BLE) Advertisements:
1. An emergency device broadcasts an SOS packet containing coordinates, casualty triage level, and timestamp.
2. Nearby peer devices running GeoRescuX receive the BLE advertisement in the background.
3. Each peer records the packet, extracts the emergency details, and immediately re-broadcasts it to neighboring peers in a store-and-forward mesh fashion until the packet reaches a responder with satellite or backhaul uplink.

### Q25. How do you prevent broadcast packet storms in the BLE mesh?
**Answer:**  
By using our C++ **Double-Buffered Rolling Bloom Filter** (`BloomFilterDeduplicator`).
In a mesh network, if every node continuously re-broadcasts every packet, the 2.4 GHz radio spectrum becomes saturated (packet storm), dropping all transmissions.
Before re-transmitting, the node checks `checkAndMark(senderHash, sequenceNumber)`:
- If the Bloom filter reports the packet was already seen, it is silently dropped in $O(1)$ time.
- If new, the bitfield is updated and the packet is forwarded.
The double-buffered design rotates every 5 minutes (`rotateEpoch()`), automatically clearing stale packets without dynamically allocating or freeing memory.

---

## Category 11: JNI Bridge & Kotlin-to-C++ Dual Layer Architecture

### Q26. What does `extern "C"` do in `GeoRescueNativeRoutingBridge.cpp`?
**Answer:**  
C++ compilers mangle function names to encode parameter types for function overloading (e.g., converting `findRoute` to `_Z9findRoutePKc`).
The Java Native Interface (JNI) requires exact, unmangled C symbol names conforming to the Java naming convention (e.g., `Java_com_example_georescux_...`).
`extern "C"` instructs the C++ compiler to use standard C linkage rules, preventing name mangling so the Android dynamic linker (`dlopen`) can locate the native function.

### Q27. What happens if the native `.so` library fails to load on an old Android phone?
**Answer:**  
GeoRescuX implements a **Defensive Fallback Architecture**:
In Kotlin, the native call is wrapped inside a try-catch block checking `GeoRescueNativeRouting.isAvailable()`.
If the native library cannot be loaded, the app automatically switches to `SafeHavenFallback.kt`, which executes an in-memory pure Kotlin routing implementation. The user interface never crashes.

---

## Category 12: Mobile Performance, Memory & Cache Optimization

### Q28. How does GeoRescuX ensure low battery consumption during a disaster?
**Answer:**  
1. **Zero Heap Allocation in Loops:** Algorithms pre-reserve memory (`bestCost.reserve(nodes.size())`) to prevent heap re-allocation and memory fragmentation.
2. **Fixed Bitfield Bloom Filters:** Deduplication uses a fixed 64 KB bitset instead of maintaining unbounded dynamic string lists.
3. **No JVM Garbage Collection (GC) Pressure:** All heavy pathfinding computations occur in native memory outside the Android ART runtime heap, eliminating UI micro-stutters and GC collection cycles.

---

## Category 13: Offline Mapping & Emergency Engineering

### Q29. How do offline maps render without internet?
**Answer:**  
GeoRescuX uses **Mapsforge binary vector map files** (`.map` format) bundled directly within the app storage for Uttarakhand, Himachal Pradesh, Haryana, and Uttar Pradesh. The OsmDroid vector renderer reads the compact binary OpenStreetMap data and renders vector road geometries directly on the GPU without downloading map image tiles from the web.

### Q30. What is the clinical triage engine in GeoRescuX?
**Answer:**  
In `app/src/main/cpp/triage/TriageEngine.cpp`, we implement the international **START (Simple Triage and Rapid Treatment)** disaster triage protocol:
- **Red (Immediate):** Life-threatening respiratory or circulatory failure.
- **Yellow (Delayed):** Serious injuries requiring care but not immediately fatal.
- **Green (Minor):** Walking wounded civilians.
- **Black (Expectant / Deceased):** Non-responsive victims without spontaneous respiration.
This categorizes casualties directly in the BLE mesh packet so rescue teams prioritize critical patients first.
