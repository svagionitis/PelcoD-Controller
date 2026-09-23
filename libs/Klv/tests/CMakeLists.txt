# ==============================================================================
# Klv Unit Tests
# ==============================================================================

function(add_klv_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${test_name} PRIVATE
        Klv
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_klv_test(TestKlvBer            TestKlvBer.cpp)
add_klv_test(TestKlvCrc            TestKlvCrc.cpp)
add_klv_test(TestKlvEncoderParser  TestKlvEncoderParser.cpp)
add_klv_test(TestKlvStreamScanner  TestKlvStreamScanner.cpp)
add_klv_test(TestMpegTsKlvExtractor TestMpegTsKlvExtractor.cpp)
add_klv_test(TestKlvGeodesy        TestKlvGeodesy.cpp)
