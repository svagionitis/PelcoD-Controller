add_library(PelcoDTracking STATIC
    PidController.h
    PidController.cpp
    PtzAutoTracker.h
    PtzAutoTracker.cpp
    PtzCameraModel.h
    PtzCameraModel.cpp
    ExtendedKalmanFilter.h
    ExtendedKalmanFilter.cpp
    UnscentedKalmanFilter.h
    UnscentedKalmanFilter.cpp
    PtzSphericalEstimator.h
    PtzSphericalEstimator.cpp
    LatencyEstimator.h
    LatencyEstimator.cpp
    LatencyCalibrator.h
    LatencyCalibrator.cpp
    OscillationDetector.h
    OscillationDetector.cpp
    PlantIdentifier.h
    PlantIdentifier.cpp
    ChirpCalibrator.h
    ChirpCalibrator.cpp
    TransientShockDetector.h
    TransientShockDetector.cpp
    PelcoDTracking.h
)

target_include_directories(PelcoDTracking PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDTracking PUBLIC
    PelcoDCore
    PelcoDMath
    Threads::Threads
    glog::glog
)

apply_compiler_flags(PelcoDTracking)
