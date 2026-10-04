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
add_klv_test(TestMpegTsKlvMuxer     TestMpegTsKlvMuxer.cpp)
add_klv_test(TestKlvGeodesy        TestKlvGeodesy.cpp)
add_klv_test(TestVmtiEncoderParser TestVmtiEncoderParser.cpp)
add_klv_test(TestMisb1201          TestMisb1201.cpp)
add_klv_test(TestFlightStreamValidation TestFlightStreamValidation.cpp)
add_klv_test(TestRvtLocalSet          TestRvtLocalSet.cpp)
add_klv_test(TestMisb0805             TestMisb0805.cpp)
add_klv_test(TestGeoRegistration      TestGeoRegistration.cpp)
add_klv_test(TestSt1607LocalSet       TestSt1607LocalSet.cpp)
add_klv_test(TestMiisCoreId           TestMiisCoreId.cpp)
add_klv_test(TestVideoNaluParser      TestVideoNaluParser.cpp)
add_klv_test(TestVideoPesPacketizer   TestVideoPesPacketizer.cpp)
add_klv_test(TestMasterTimeBase       TestMasterTimeBase.cpp)
add_klv_test(TestPtsSyncManager       TestPtsSyncManager.cpp)

