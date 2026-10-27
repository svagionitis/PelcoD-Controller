# ==============================================================================
# Math Unit Tests
# ==============================================================================

function(add_math_test test_name test_source)
    add_executable(${test_name} ${test_source})
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
    )
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${test_name} PRIVATE Math)
    pelcod_register_gtest(${test_name})
endfunction()

add_math_test(TestFft                  TestFft.cpp)
add_math_test(TestDctSharpness         TestDctSharpness.cpp)
add_math_test(TestGoertzelFilter       TestGoertzelFilter.cpp)
add_math_test(TestIntegralImage        TestIntegralImage.cpp)
add_math_test(TestPhaseCorrelation     TestPhaseCorrelation.cpp)
add_math_test(TestStft                 TestStft.cpp)
add_math_test(TestNotchFilter          TestNotchFilter.cpp)
add_math_test(TestSpectrogramColorMap  TestSpectrogramColorMap.cpp)
add_math_test(TestDct                  TestDct.cpp)
add_math_test(TestGabor                TestGabor.cpp)
add_math_test(TestDwt                  TestDwt.cpp)
add_math_test(TestMatrixMath           TestMatrixMath.cpp)
