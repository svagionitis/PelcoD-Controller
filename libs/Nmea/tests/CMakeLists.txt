# ==============================================================================
# Nmea Unit Tests
# ==============================================================================

function(add_nmea_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${test_name} PRIVATE
        Nmea
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_nmea_test(TestNmeaChecksum         TestNmeaChecksum.cpp)
add_nmea_test(TestNmeaSentenceParser   TestNmeaSentenceParser.cpp)
add_nmea_test(TestNmeaSentenceBuilder  TestNmeaSentenceBuilder.cpp)
add_nmea_test(TestNmeaStreamAccumulator TestNmeaStreamAccumulator.cpp)
add_nmea_test(TestAisBitReader         TestAisBitReader.cpp)
add_nmea_test(TestAisDecoder           TestAisDecoder.cpp)
add_nmea_test(TestNmeaDevice           TestNmeaDevice.cpp)
add_nmea_test(TestFlirPfecDevice       TestFlirPfecDevice.cpp)
add_nmea_test(TestNmeaTagBlock          TestNmeaTagBlock.cpp)
add_nmea_test(TestNmeaMaritimeSentences TestNmeaMaritimeSentences.cpp)
add_nmea_test(TestN2kFastPacket         TestN2kFastPacket.cpp)
add_nmea_test(TestN2kDecoder            TestN2kDecoder.cpp)
add_nmea_test(TestN2kDevice             TestN2kDevice.cpp)
add_nmea_test(TestLweMulticastTransport TestLweMulticastTransport.cpp)
add_nmea_test(TestLweChannelManager     TestLweChannelManager.cpp)
