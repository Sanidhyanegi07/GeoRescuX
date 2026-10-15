cmake_minimum_required(VERSION 3.16)
project(georescux_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

set(BACKEND_SOURCES
    main.cc
    storage/SqliteStorage.cc
    filters/FirebaseAuthFilter.cc
    controllers/HealthController.cc
    controllers/SosEventController.cc
    controllers/BleMeshController.cc
    controllers/ClusterController.cc
    spatial/DbscanClusterer.cc
    spatial/DisasterSpreadSimulator.cc
    spatial/WildfireRothermelModel.cc
    spatial/HydrodynamicFloodModel.cc
    spatial/AvalancheSnowpackModel.cc
    spatial/LandslideSusceptibilityEngine.cc
    spatial/SeismicWaveAttenuationModel.cc
    spatial/PlumeDispersionModel.cc
    dispatch/RescueResourceMatcher.cc
    dispatch/RescueDispatchOptimizer.cc
    mesh/MeshIngestionPipeline.cc
    realtime/RescueCommandBroker.cc
)

add_executable(georescux_backend ${BACKEND_SOURCES})

target_link_libraries(georescux_backend
    PRIVATE
    Drogon::Drogon
    ${SQLite3_LIBRARIES}
)

# Test executables
add_executable(backend_test
    test/backend_test.cc
    storage/SqliteStorage.cc
    filters/FirebaseAuthFilter.cc
    spatial/DbscanClusterer.cc
    spatial/DisasterSpreadSimulator.cc
    dispatch/RescueResourceMatcher.cc
    mesh/MeshIngestionPipeline.cc
)

target_link_libraries(backend_test
    PRIVATE
    Drogon::Drogon
    ${SQLite3_LIBRARIES}
)

add_executable(disaster_simulation_test
    test/disaster_simulation_test.cc
    spatial/WildfireRothermelModel.cc
    spatial/HydrodynamicFloodModel.cc
    spatial/AvalancheSnowpackModel.cc
    spatial/LandslideSusceptibilityEngine.cc
    spatial/SeismicWaveAttenuationModel.cc
    spatial/PlumeDispersionModel.cc
    dispatch/RescueDispatchOptimizer.cc
    realtime/RescueCommandBroker.cc
)

target_link_libraries(disaster_simulation_test
    PRIVATE
    ${SQLite3_LIBRARIES}
)

enable_testing()
add_test(NAME BackendUnitTests COMMAND backend_test)
add_test(NAME DisasterSimulationTests COMMAND disaster_simulation_test)
