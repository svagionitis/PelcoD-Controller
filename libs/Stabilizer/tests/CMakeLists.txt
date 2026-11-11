# ==============================================================================
# Stabilizer Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_stabilizer_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        Stabilizer
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_stabilizer_test(TestStabilizerTypes TestStabilizerTypes.cpp)
add_stabilizer_test(TestFrameView       TestFrameView.cpp)
add_stabilizer_test(TestStabilizerArena TestStabilizerArena.cpp)
add_stabilizer_test(TestFramePyramid   TestFramePyramid.cpp)
add_stabilizer_test(TestFeatureDetector TestFeatureDetector.cpp)
add_stabilizer_test(TestOpticalFlowTracker TestOpticalFlowTracker.cpp)

