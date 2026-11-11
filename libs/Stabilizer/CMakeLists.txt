add_library(Stabilizer STATIC
    StabilizerTypes.h
    FrameView.h
    FrameBuffer.h
    FrameBuffer.cpp
    StabilizerArena.h
    StabilizerArena.cpp
    FramePyramid.h
    FramePyramid.cpp
    FeatureDetector.h
    FeatureDetector.cpp
    OpticalFlowTracker.h
    OpticalFlowTracker.cpp
    MotionModelEstimator.h
    MotionModelEstimator.cpp
    TrajectoryFilter.h
    TrajectoryFilter.cpp
)

target_include_directories(Stabilizer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Stabilizer PUBLIC
    Math
    Threads::Threads
)

apply_compiler_flags(Stabilizer)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

