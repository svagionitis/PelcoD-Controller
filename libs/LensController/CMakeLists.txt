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
    AutoFocusTypes.h
    PeakFitter.h
    PeakFitter.cpp
    AutoFocusEngine.h
    AutoFocusEngine.cpp
    LensDriverLoader.h
    LensDriverLoader.cpp
    ViscaLensDriver.h
    ViscaLensDriver.cpp
    PelcoDLensDriver.h
    PelcoDLensDriver.cpp
    LensController.h
    LensController.cpp
    LensPayloadAdapter.h
    LensPayloadAdapter.cpp
)

target_include_directories(LensController PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../PayloadHal>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(LensController PUBLIC
    PayloadHal
    Math
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

apply_compiler_flags(LensController)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

