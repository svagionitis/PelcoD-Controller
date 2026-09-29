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
add_nmea_test(TestN2kAddressClaimer     TestN2kAddressClaimer.cpp)
add_nmea_test(TestLweMulticastTransport TestLweMulticastTransport.cpp)
add_nmea_test(TestLweChannelManager     TestLweChannelManager.cpp)
add_nmea_test(TestAisEmergencyBeacon    TestAisEmergencyBeacon.cpp)
add_nmea_test(TestNmeaReplayTransport   TestNmeaReplayTransport.cpp)
add_nmea_test(TestNmeaRouteAndSar       TestNmeaRouteAndSar.cpp)
target_link_libraries(TestNmeaRouteAndSar PRIVATE PayloadHal)
add_nmea_test(TestNmeaEnvironmentalTuning TestNmeaEnvironmentalTuning.cpp)
add_nmea_test(TestN2kEncoder            TestN2kEncoder.cpp)
add_nmea_test(TestNmeaGateway           TestNmeaGateway.cpp)
add_nmea_test(TestNmeaSensorArbiter     TestNmeaSensorArbiter.cpp)
add_nmea_test(TestBridgeAlertManager    TestBridgeAlertManager.cpp)
add_nmea_test(TestNetworkCrypto             TestNetworkCrypto.cpp)
add_nmea_test(TestIec61162_460Firewall      TestIec61162_460Firewall.cpp)
add_nmea_test(TestNmeaTcpServer             TestNmeaTcpServer.cpp)
add_nmea_test(TestNmeaUdpEndpoint           TestNmeaUdpEndpoint.cpp)
add_nmea_test(TestNmeaJsonSerializer        TestNmeaJsonSerializer.cpp)
add_nmea_test(TestNmeaWebSocketServer       TestNmeaWebSocketServer.cpp)
add_nmea_test(TestMaritimeNetworkCoordinator TestMaritimeNetworkCoordinator.cpp)
