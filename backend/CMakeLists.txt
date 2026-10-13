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
)

add_executable(georescux_backend ${BACKEND_SOURCES})

target_link_libraries(georescux_backend
    PRIVATE
    Drogon::Drogon
    ${SQLite3_LIBRARIES}
)

# Test executable
add_executable(backend_test
    test/backend_test.cc
    storage/SqliteStorage.cc
    filters/FirebaseAuthFilter.cc
)

target_link_libraries(backend_test
    PRIVATE
    Drogon::Drogon
    ${SQLite3_LIBRARIES}
)

enable_testing()
add_test(NAME BackendUnitTests COMMAND backend_test)
