add_library(PelcoDMath STATIC
    Dct.h
    Dct.cpp
    DctSharpnessEvaluator.h
    DctSharpnessEvaluator.cpp
    Dwt.h
    Dwt.cpp
    Fft.h
    Fft.cpp
    GoertzelFilter.h
    GoertzelFilter.cpp
    IntegralImage.h
    IntegralImage.cpp
    MatrixMath.h
    NotchFilter.h
    NotchFilter.cpp
    PhaseCorrelation.h
    PhaseCorrelation.cpp
    SpectrogramColorMap.h
    SpectrogramColorMap.cpp
    Stft.h
    Stft.cpp
)

target_include_directories(PelcoDMath PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDMath PUBLIC
    Threads::Threads
)

apply_compiler_flags(PelcoDMath)
