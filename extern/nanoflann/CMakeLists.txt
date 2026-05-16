cmake_minimum_required(VERSION 3.1)
project(nanoflann VERSION 1.7.1 LANGUAGES CXX)

# Options expected by CCCoreLib (we honour them by ignoring the corresponding subdirs)
option(NANOFLANN_BUILD_EXAMPLES  "Build examples"   ON)
option(NANOFLANN_BUILD_TESTS     "Build tests"      ON)
option(NANOFLANN_BUILD_BENCHMARKS "Build benchmarks" ON)

add_library(nanoflann INTERFACE)
add_library(nanoflann::nanoflann ALIAS nanoflann)

target_include_directories(nanoflann
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
