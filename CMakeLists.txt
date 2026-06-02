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

# ── Platform-specific settings ───────────────────────────────────────────────
if(WIN32)
    # Windows: Suppress min/max macros from windows.h (conflict with std::min/max)
    add_definitions(-DNOMINMAX)
    # Windows: Use UTF-8 source encoding for MSVC
    if(MSVC)
        add_compile_options(/utf-8)
        # Disable some noisy warnings
        add_compile_options(/wd4251 /wd4275)  # DLL export warnings
    endif()
endif()

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Concurrent)

# ── STL Import Feature (VTK, CGAL, Eigen) ────────────────────────────────────
# Enable/disable STL import feature (requires VTK, CGAL, Eigen)
option(MATCH3D_ENABLE_STL_IMPORT "Enable STL file import with 3D preview" ON)

if(MATCH3D_ENABLE_STL_IMPORT)
    # VTK: User can set VTK_DIR via cmake -DVTK_DIR=...
    # Linux default hint: ~/VTK-install-linux/lib/cmake/vtk-9.3
    # Windows: typically C:/VTK/lib/cmake/vtk-9.3 or via vcpkg
    if(NOT VTK_DIR)
        if(UNIX AND NOT APPLE)
            set(VTK_DIR "$ENV{HOME}/VTK-install-linux/lib/cmake/vtk-9.3" CACHE PATH "VTK cmake directory")
        endif()
    endif()

    find_package(VTK 9.3 REQUIRED COMPONENTS
        CommonCore
        CommonDataModel
        CommonTransforms
        FiltersSources
        FiltersGeneral
        FiltersCore
        InteractionStyle
        InteractionWidgets
        RenderingCore
        RenderingOpenGL2
        RenderingAnnotation
        GUISupportQt
    )
    message(STATUS "VTK ${VTK_VERSION} found at ${VTK_DIR}")

    # CGAL: Header-only mode is sufficient for our use
    find_package(CGAL 6.0 REQUIRED)
    message(STATUS "CGAL ${CGAL_VERSION} found")

    # Eigen: Required for transformation matrices
    find_package(Eigen3 3.4 REQUIRED CONFIG)
    message(STATUS "Eigen ${Eigen3_VERSION} found")

    # Export flag for conditional compilation
    add_compile_definitions(MATCH3D_STL_IMPORT_ENABLED)
endif()

# CCCoreLib: build as static lib, use Qt::Concurrent for parallelism
set(CCCORELIB_SHARED          OFF CACHE BOOL "" FORCE)
set(CCCORELIB_USE_QT_CONCURRENT ON  CACHE BOOL "" FORCE)
set(CCCORELIB_USE_CGAL        OFF CACHE BOOL "" FORCE)
set(CCCORELIB_USE_TBB         OFF CACHE BOOL "" FORCE)

add_subdirectory(extern/CCCoreLib)

add_subdirectory(src)
