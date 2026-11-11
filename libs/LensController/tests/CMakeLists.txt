# ==============================================================================
# LensController Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_lens_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        LensController
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_lens_test(TestLensTypes      TestLensTypes.cpp)
add_lens_test(TestMockLensDriver TestMockLensDriver.cpp)
add_lens_test(TestFocusEvaluator TestFocusEvaluator.cpp)
add_lens_test(TestZoomTrackCurve TestZoomTrackCurve.cpp)

