cmake_minimum_required(VERSION 3.22.1)
project("georescux_routing" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(georescux_routing SHARED
    routing/RouteGraph.cpp
    routing/AStarPathfinder.cpp
    routing/BidirectionalAStar.cpp
    routing/DynamicGraphUpdater.cpp
    routing/TurnPenaltyEngine.cpp
    routing/ContractionHierarchies.cpp
    routing/AlternativeRouteFinder.cpp
    routing/IsochroneGenerator.cpp
    routing/MultiModalEvacuation.cpp
    routing/DynamicTollRiskRouter.cpp
    routing/SafeHavenFallback.cpp
    routing/GraphJsonParser.cpp
    spatial/GeodesicMath.cpp
    spatial/GeodesicPolygon.cpp
    spatial/KDTree.cpp
    spatial/HazardPolygon.cpp
    spatial/ElevationCostModel.cpp
    spatial/DigitalElevationModel.cpp
    spatial/SpatialJoinEngine.cpp
    spatial/VoronoiTessellation.cpp
    spatial/SpatialClusteringEngine.cpp
    spatial/VectorTileRasterizer.cpp
    spatial/LineOfSightElevationEngine.cpp
    mesh/MeshPacketCodec.cpp
    mesh/BloomFilterDeduplicator.cpp
    mesh/EpidemicRouter.cpp
    mesh/PacketFragmenter.cpp
    mesh/SlidingWindowReplayGuard.cpp
    mesh/VectorClock.cpp
    mesh/MeshGossipProtocol.cpp
    mesh/ReliableBleTransport.cpp
    telemetry/DeadReckoningEngine.cpp
    telemetry/KalmanFusedLocator.cpp
    audio/AcousticResonanceDetector.cpp
    analytics/CasualtySurvivalProbability.cpp
    network/LossResilientFecCodec.cpp
    network/BandwidthAdaptiveCompressor.cpp
    triage/TriageEngine.cpp
    triage/MedicalTriageDecisionTree.cpp
    crypto/PacketAuthenticator.cpp
    jni/GeoRescueNativeRoutingBridge.cpp
)

target_include_directories(georescux_routing PRIVATE
    routing
    spatial
    mesh
    telemetry
    audio
    analytics
    network
    triage
    crypto
    jni
)

find_library(log-lib log)
if(log-lib)
    target_link_libraries(georescux_routing PRIVATE ${log-lib})
endif()
