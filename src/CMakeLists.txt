# ── VIFF I/O library (shared between main app and test) ───────────────────────
add_library(viff_io STATIC
    io/ViffReader.h
    io/ViffReader.cpp
    io/ViffWriter.h
    io/ViffWriter.cpp
)
target_include_directories(viff_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(viff_io PUBLIC cxx_std_20)

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

# ── VIFF round-trip test (no Qt dependency) ───────────────────────────────────
add_executable(viff_test io/viff_test.cpp)
target_link_libraries(viff_test PRIVATE viff_io)

# ── Synthetic test data generator ─────────────────────────────────────────────
add_executable(synthetic_test_data tools/SyntheticTestData.cpp)
target_link_libraries(synthetic_test_data PRIVATE viff_io)

# ── Wear sample generator (plane+depression, plane+dome) ─────────────────────
add_executable(generate_wear_samples tools/GenerateWearSamples.cpp)
target_link_libraries(generate_wear_samples PRIVATE viff_io)
