add_library(LensController STATIC
    LensTypes.h
    ILensDriver.h
    MockLensDriver.h
    MockLensDriver.cpp
    FocusEvaluator.h
    FocusEvaluator.cpp
    ZoomTrackTypes.h
    ZoomTrackFocusEngine.h
    ZoomTrackFocusEngine.cpp
)

target_include_directories(LensController PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(LensController PUBLIC
    Math
    Threads::Threads
)

apply_compiler_flags(LensController)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

