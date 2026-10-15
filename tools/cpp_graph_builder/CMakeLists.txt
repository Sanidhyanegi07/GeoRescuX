cmake_minimum_required(VERSION 3.16)
project(cpp_graph_builder CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cpp_graph_builder
    main.cpp
    OsmPbfReader.cpp
    GraphExtractor.cpp
)

target_include_directories(cpp_graph_builder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
