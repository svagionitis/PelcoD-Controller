# ==============================================================================
# PelcoDTracking Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_tracking_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
        ${CMAKE_SOURCE_DIR}/libs/PelcoDMath
        ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
        ${CMAKE_SOURCE_DIR}/libs/PelcoDTransport
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDTracking
        PelcoDMath
        PelcoDSim
        PelcoDTransport
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_tracking_test(TestPidController        TestPidController.cpp)
add_tracking_test(TestLatencyEstimator     TestLatencyEstimator.cpp)
add_tracking_test(TestOscillationDetector  TestOscillationDetector.cpp)
add_tracking_test(TestNonlinearKalman      TestNonlinearKalman.cpp)
add_tracking_test(TestMockKinematics       TestMockKinematics.cpp)
add_tracking_test(TestPlantIdentifier      TestPlantIdentifier.cpp)
add_tracking_test(TestChirpCalibrator      TestChirpCalibrator.cpp)
add_tracking_test(TestLatencyCalibrator    TestLatencyCalibrator.cpp)
add_tracking_test(TestPtzCameraModel       TestPtzCameraModel.cpp)
add_tracking_test(TestPtzSphericalEstimator TestPtzSphericalEstimator.cpp)
add_tracking_test(TestTransientShockDetector TestTransientShockDetector.cpp)
add_tracking_test(TestExtendedKalmanFilter TestExtendedKalmanFilter.cpp)
add_tracking_test(TestUnscentedKalmanFilter TestUnscentedKalmanFilter.cpp)
