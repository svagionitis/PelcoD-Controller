# ==============================================================================
# SightlineCore Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_sightline_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_SOURCE_DIR}/libs/Transport
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        SightlineCore
        Transport
        PelcoDTestSupport
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_sightline_test(TestSightlineCrc8            TestSightlineCrc8.cpp)
add_sightline_test(TestSightlineAccumulator     TestSightlineAccumulator.cpp)
add_sightline_test(TestSightlineDevice          TestSightlineDevice.cpp)
add_sightline_test(TestSightlineTracking        TestSightlineTracking.cpp)
add_sightline_test(TestSightlineStabilization   TestSightlineStabilization.cpp)
add_sightline_test(TestSightlineVideoPipeline   TestSightlineVideoPipeline.cpp)
add_sightline_test(TestSightlineSystem          TestSightlineSystem.cpp)
add_sightline_test(TestSightlineTelemetryKLV    TestSightlineTelemetryKLV.cpp)
add_sightline_test(TestSightlineHardwareOverlay TestSightlineHardwareOverlay.cpp)
add_sightline_test(TestSightlinePeripherals     TestSightlinePeripherals.cpp)
add_sightline_test(TestSightlineZeroCopy        TestSightlineZeroCopy.cpp)
add_sightline_test(TestSightlineRadiometry      TestSightlineRadiometry.cpp)
add_sightline_test(TestSightlineIsotherm        TestSightlineIsotherm.cpp)
add_sightline_test(TestSightlineKlvRadiometry   TestSightlineKlvRadiometry.cpp)
add_sightline_test(TestSightlineNuc             TestSightlineNuc.cpp)
add_sightline_test(TestSightlineNucCaps         TestSightlineNucCaps.cpp)
add_sightline_test(TestSightlineNucWorkflow     TestSightlineNucWorkflow.cpp)
add_sightline_test(TestSightlineNucTableFile    TestSightlineNucTableFile.cpp)


