cmake_minimum_required(VERSION 3.22.1)
project("georescux_routing" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(georescux_routing SHARED
    routing/RouteGraph.cpp
    routing/AStarPathfinder.cpp
    routing/SafeHavenFallback.cpp
    routing/GraphJsonParser.cpp
    jni/GeoRescueNativeRoutingBridge.cpp
)

target_include_directories(georescux_routing PRIVATE
    routing
    jni
)

find_library(log-lib log)
if(log-lib)
    target_link_libraries(georescux_routing PRIVATE ${log-lib})
endif()
