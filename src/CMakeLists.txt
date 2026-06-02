# ── VIFF I/O library (shared between main app and test) ───────────────────────
add_library(viff_io STATIC
    io/ViffReader.h
    io/ViffReader.cpp
    io/ViffWriter.h
    io/ViffWriter.cpp
)
target_include_directories(viff_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(viff_io PUBLIC cxx_std_20)

# ── STL Import Libraries (conditional) ────────────────────────────────────────
if(MATCH3D_ENABLE_STL_IMPORT)
    # Ensure Eigen3 target is available (needed for CGAL and transformations)
    if(NOT TARGET Eigen3::Eigen)
        find_package(Eigen3 3.4 REQUIRED CONFIG)
    endif()

    # Mesh3D Core: STL reading, mesh data structures, projection
    add_library(mesh3d_core STATIC
        mesh3d/MeshData.h
        mesh3d/STLReader.h
        mesh3d/STLReader.cpp
        mesh3d/MeshProjection.h
        mesh3d/MeshProjection.cpp
    )
    target_include_directories(mesh3d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(mesh3d_core
        PUBLIC
            CGAL::CGAL
            Eigen3::Eigen
        PRIVATE
            viff_io
    )
    target_compile_features(mesh3d_core PUBLIC cxx_std_20)

    # Platform-specific settings for mesh3d_core
    if(WIN32 AND MSVC)
        # CGAL on Windows may need BigObj for large template instantiations
        target_compile_options(mesh3d_core PRIVATE /bigobj)
    endif()

    # Visualization3D: VTK-based 3D preview widget
    add_library(viz3d STATIC
        visualization3d/STLPreviewWidget.h
        visualization3d/STLPreviewWidget.cpp
    )
    target_include_directories(viz3d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(viz3d
        PUBLIC
            mesh3d_core
            Qt6::Widgets
            ${VTK_LIBRARIES}
    )
    target_compile_features(viz3d PUBLIC cxx_std_20)

    # VTK module auto-init (required for VTK factory registration)
    vtk_module_autoinit(TARGETS viz3d MODULES ${VTK_LIBRARIES})
endif()

# ── Main application ───────────────────────────────────────────────────────────
add_executable(match3d_plus
    main.cpp
    MainWindow.h
    MainWindow.cpp
    ImageWindow.h
    ImageWindow.cpp
    AppSettings.h
    AppSettings.cpp
    DepthImageView.h
    DepthImageView.cpp
    RoiMask.h
    RoiMask.cpp
    ImageProcessor.h
    ImageProcessor.cpp
    AppSettings.h
    dialogs/GlobalParametersDialog.h
    dialogs/GlobalParametersDialog.cpp
    dialogs/HistogramDialog.h
    dialogs/HistogramDialog.cpp
    dialogs/MatchingControlPanel.h
    dialogs/MatchingControlPanel.cpp
    registration/Transformation3D.h
    registration/CoarseRegistration.h
    registration/CoarseRegistration.cpp
    registration/RegistrationWorker.h
    registration/RegistrationWorker.cpp
    registration/DifferenceCalculator.h
    registration/DifferenceCalculator.cpp
    registration/SVD3x3.h
    registration/SVD3x3.cpp
    io/PlyIO.h
    io/PlyIO.cpp
)

target_link_libraries(match3d_plus
    PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
        CCCoreLib::CCCoreLib
        viff_io
)

target_include_directories(match3d_plus
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/extern/happly
)

# ── STL Import: Add dialog and link libraries ─────────────────────────────────
if(MATCH3D_ENABLE_STL_IMPORT)
    target_sources(match3d_plus PRIVATE
        dialogs/STLImportDialog.h
        dialogs/STLImportDialog.cpp
    )
    target_link_libraries(match3d_plus PRIVATE mesh3d_core viz3d)
    # VTK auto-init for main executable as well
    vtk_module_autoinit(TARGETS match3d_plus MODULES ${VTK_LIBRARIES})
endif()

# ── VIFF round-trip test (no Qt dependency) ───────────────────────────────────
add_executable(viff_test io/viff_test.cpp)
target_link_libraries(viff_test PRIVATE viff_io)

# ── Synthetic test data generator ─────────────────────────────────────────────
add_executable(synthetic_test_data tools/SyntheticTestData.cpp)
target_link_libraries(synthetic_test_data PRIVATE viff_io)

# ── Wear sample generator (plane+depression, plane+dome) ─────────────────────
add_executable(generate_wear_samples tools/GenerateWearSamples.cpp)
target_link_libraries(generate_wear_samples PRIVATE viff_io)
