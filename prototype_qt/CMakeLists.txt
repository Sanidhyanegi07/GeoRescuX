cmake_minimum_required(VERSION 3.22)
project(GeoRescuXQtPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MinGWConfig)
include(AndroidConfig)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick)
qt_standard_project_setup(REQUIRES 6.8)

# -----------------------------------------------------------------------------
# Main Application Target
# -----------------------------------------------------------------------------
qt_add_executable(appGeoRescuXQtPrototype
    src/app/main.cpp
    src/platform/qt/PrototypeBackend.h
    src/platform/qt/PrototypeBackend.cpp
    ../app/src/main/cpp/spatial/GeodesicMath.cpp
)

target_include_directories(appGeoRescuXQtPrototype PRIVATE
    src
    src/platform/qt
    ../app/src/main/cpp/spatial
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
)

georescux_configure_android_target(appGeoRescuXQtPrototype)

# -----------------------------------------------------------------------------
# Unit Testing Target
# -----------------------------------------------------------------------------
enable_testing()

add_executable(tst_geodesic
    tests/tst_geodesic.cpp
    ../app/src/main/cpp/spatial/GeodesicMath.cpp
)

target_include_directories(tst_geodesic PRIVATE
    ../app/src/main/cpp/spatial
)

add_test(NAME GeodesicMathTest COMMAND tst_geodesic)
