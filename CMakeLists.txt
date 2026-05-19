cmake_minimum_required(VERSION 3.20)

project(match3d_plus
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent)

# CCCoreLib: build as static lib, use Qt::Concurrent for parallelism
set(CCCORELIB_SHARED          OFF CACHE BOOL "" FORCE)
set(CCCORELIB_USE_QT_CONCURRENT ON  CACHE BOOL "" FORCE)
set(CCCORELIB_USE_CGAL        OFF CACHE BOOL "" FORCE)
set(CCCORELIB_USE_TBB         OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/CCCoreLib)

add_subdirectory(src)
