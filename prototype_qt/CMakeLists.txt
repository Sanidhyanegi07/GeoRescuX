cmake_minimum_required(VERSION 3.22)
project(GeoRescuXQtPrototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Quick)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appGeoRescuXQtPrototype
    src/main.cpp
    src/PrototypeBackend.h
    src/PrototypeBackend.cpp
    ../app/src/main/cpp/spatial/GeodesicMath.cpp
)

target_include_directories(appGeoRescuXQtPrototype PRIVATE
    src
    ../app/src/main/cpp/spatial
)

qt_add_qml_module(appGeoRescuXQtPrototype
    URI GeoRescuX.Prototype
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(appGeoRescuXQtPrototype PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
)

if(ANDROID)
    set_target_properties(appGeoRescuXQtPrototype PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.example.georescux.qtprototype"
        QT_ANDROID_PACKAGE_VERSION_CODE "1"
        QT_ANDROID_PACKAGE_VERSION_NAME "1.0.0"
        QT_ANDROID_MIN_SDK_VERSION "28"
        QT_ANDROID_TARGET_SDK_VERSION "35"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    )
endif()
