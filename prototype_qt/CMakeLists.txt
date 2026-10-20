cmake_minimum_required(VERSION 3.22)
project(GeoRescuXQtPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MinGWConfig)
include(AndroidConfig)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick Sql)
qt_standard_project_setup(REQUIRES 6.8)

set(GEORESCUX_CORE_SOURCES
    ../app/src/main/cpp/routing/RouteGraph.cpp
    ../app/src/main/cpp/routing/AStarPathfinder.cpp
    ../app/src/main/cpp/routing/BidirectionalAStar.cpp
    ../app/src/main/cpp/routing/DynamicGraphUpdater.cpp
    ../app/src/main/cpp/routing/TurnPenaltyEngine.cpp
    ../app/src/main/cpp/routing/ContractionHierarchies.cpp
    ../app/src/main/cpp/routing/AlternativeRouteFinder.cpp
    ../app/src/main/cpp/routing/IsochroneGenerator.cpp
    ../app/src/main/cpp/routing/MultiModalEvacuation.cpp
    ../app/src/main/cpp/routing/DynamicTollRiskRouter.cpp
    ../app/src/main/cpp/routing/SafeHavenFallback.cpp
    ../app/src/main/cpp/routing/GraphJsonParser.cpp
    ../app/src/main/cpp/spatial/GeodesicMath.cpp
    ../app/src/main/cpp/spatial/GeodesicPolygon.cpp
    ../app/src/main/cpp/spatial/KDTree.cpp
    ../app/src/main/cpp/spatial/HazardPolygon.cpp
    ../app/src/main/cpp/spatial/ElevationCostModel.cpp
    ../app/src/main/cpp/spatial/DigitalElevationModel.cpp
    ../app/src/main/cpp/spatial/SpatialJoinEngine.cpp
    ../app/src/main/cpp/spatial/VoronoiTessellation.cpp
    ../app/src/main/cpp/spatial/SpatialClusteringEngine.cpp
    ../app/src/main/cpp/spatial/VectorTileRasterizer.cpp
    ../app/src/main/cpp/spatial/LineOfSightElevationEngine.cpp
    ../app/src/main/cpp/mesh/MeshPacketCodec.cpp
    ../app/src/main/cpp/mesh/BloomFilterDeduplicator.cpp
    ../app/src/main/cpp/mesh/EpidemicRouter.cpp
    ../app/src/main/cpp/mesh/PacketFragmenter.cpp
    ../app/src/main/cpp/mesh/SlidingWindowReplayGuard.cpp
    ../app/src/main/cpp/mesh/VectorClock.cpp
    ../app/src/main/cpp/mesh/MeshGossipProtocol.cpp
    ../app/src/main/cpp/mesh/ReliableBleTransport.cpp
    ../app/src/main/cpp/telemetry/DeadReckoningEngine.cpp
    ../app/src/main/cpp/telemetry/KalmanFusedLocator.cpp
    ../app/src/main/cpp/audio/AcousticResonanceDetector.cpp
    ../app/src/main/cpp/analytics/CasualtySurvivalProbability.cpp
    ../app/src/main/cpp/network/LossResilientFecCodec.cpp
    ../app/src/main/cpp/network/BandwidthAdaptiveCompressor.cpp
    ../app/src/main/cpp/triage/TriageEngine.cpp
    ../app/src/main/cpp/triage/MedicalTriageDecisionTree.cpp
    ../app/src/main/cpp/crypto/PacketAuthenticator.cpp
)

# -----------------------------------------------------------------------------
# Main Application Target
# -----------------------------------------------------------------------------
qt_add_executable(appGeoRescuXQtPrototype
    src/app/main.cpp
    src/app/AppState.h
    src/app/AppState.cpp
    src/domain/SessionModel.h
    src/domain/SessionModel.cpp
    src/domain/EmergencyModel.h
    src/domain/EmergencyModel.cpp
    src/domain/IEmergencyRepository.h
    src/domain/EmergencyService.h
    src/domain/EmergencyService.cpp
    src/platform/storage/LocalDatabase.h
    src/platform/storage/LocalDatabase.cpp
    src/platform/storage/SqliteEmergencyRepository.h
    src/platform/storage/SqliteEmergencyRepository.cpp
    src/platform/qt/PrototypeBackend.h
    src/platform/qt/PrototypeBackend.cpp
    ${GEORESCUX_CORE_SOURCES}
)

target_include_directories(appGeoRescuXQtPrototype PRIVATE
    src
    src/app
    src/domain
    src/platform/storage
    src/platform/qt
    ../app/src/main/cpp/routing
    ../app/src/main/cpp/spatial
    ../app/src/main/cpp/mesh
    ../app/src/main/cpp/telemetry
    ../app/src/main/cpp/audio
    ../app/src/main/cpp/analytics
    ../app/src/main/cpp/network
    ../app/src/main/cpp/triage
    ../app/src/main/cpp/crypto
)

target_compile_options(appGeoRescuXQtPrototype PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-includealgorithm>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-includecmath>
)

qt_add_qml_module(appGeoRescuXQtPrototype
    URI GeoRescuX.Prototype
    VERSION 1.0
    QML_FILES
        ui/qml/Main.qml
)

target_link_libraries(appGeoRescuXQtPrototype PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Sql
)

georescux_configure_android_target(appGeoRescuXQtPrototype)

# -----------------------------------------------------------------------------
# Unit Testing Targets (Desktop Host Only)
# -----------------------------------------------------------------------------
if(NOT ANDROID)
    enable_testing()

    add_executable(tst_geodesic
        tests/tst_geodesic.cpp
        ../app/src/main/cpp/spatial/GeodesicMath.cpp
    )

    target_include_directories(tst_geodesic PRIVATE
        ../app/src/main/cpp/spatial
    )

    add_test(NAME GeodesicMathTest COMMAND tst_geodesic)

    add_executable(tst_foundation
        tests/tst_foundation.cpp
        src/domain/SessionModel.cpp
        src/domain/SessionModel.h
        src/domain/EmergencyModel.cpp
        src/domain/EmergencyModel.h
        src/domain/IEmergencyRepository.h
        src/domain/EmergencyService.cpp
        src/domain/EmergencyService.h
        src/platform/storage/LocalDatabase.cpp
        src/platform/storage/LocalDatabase.h
        src/platform/storage/SqliteEmergencyRepository.cpp
        src/platform/storage/SqliteEmergencyRepository.h
        src/app/AppState.cpp
        src/app/AppState.h
    )

    target_include_directories(tst_foundation PRIVATE
        src
        src/app
        src/domain
        src/platform/storage
    )

    target_link_libraries(tst_foundation PRIVATE
        Qt6::Core
        Qt6::Sql
    )

    add_test(NAME FoundationTest COMMAND tst_foundation)

    add_executable(tst_emergency
        tests/tst_emergency.cpp
        src/domain/EmergencyModel.cpp
        src/domain/EmergencyModel.h
        src/domain/IEmergencyRepository.h
        src/domain/EmergencyService.cpp
        src/domain/EmergencyService.h
        src/domain/SessionModel.cpp
        src/domain/SessionModel.h
        src/platform/storage/LocalDatabase.cpp
        src/platform/storage/LocalDatabase.h
        src/platform/storage/SqliteEmergencyRepository.cpp
        src/platform/storage/SqliteEmergencyRepository.h
        src/app/AppState.cpp
        src/app/AppState.h
    )

    target_include_directories(tst_emergency PRIVATE
        src
        src/app
        src/domain
        src/platform/storage
    )

    target_link_libraries(tst_emergency PRIVATE
        Qt6::Core
        Qt6::Sql
    )

    add_test(NAME EmergencyLifecycleTest COMMAND tst_emergency)
endif()
