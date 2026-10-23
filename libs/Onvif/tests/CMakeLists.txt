# ==============================================================================
# PelcoDOnvif Unit Tests  (requires PELCOD_ENABLE_ONVIF)
# ==============================================================================

function(add_onvif_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
    )
    target_link_libraries(${test_name} PRIVATE
        Onvif
        PelcoDCore
        PelcoDSim
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_onvif_test(TestPelcoDOnvif  TestPelcoDOnvif.cpp)
add_onvif_test(TestOnvifServer  TestOnvifServer.cpp)
add_onvif_test(TestOnvifAuthServer  TestOnvifAuthServer.cpp)
add_onvif_test(TestOnvifAuth  TestOnvifAuth.cpp)
add_onvif_test(TestXmlUtils  TestXmlUtils.cpp)
add_onvif_test(TestOnvifXmlSecurity TestOnvifXmlSecurity.cpp)
add_onvif_test(TestOnvifEventSecurity TestOnvifEventSecurity.cpp)
add_onvif_test(TestOnvifCredentialsSecurity TestOnvifCredentialsSecurity.cpp)
add_onvif_test(TestOnvifDiscoverySecurity TestOnvifDiscoverySecurity.cpp)
add_onvif_test(TestOnvifServicesConformance TestOnvifServicesConformance.cpp)
add_onvif_test(TestOnvifSoapFault TestOnvifSoapFault.cpp)
add_onvif_test(TestOnvifServerLifecycle TestOnvifServerLifecycle.cpp)
add_onvif_test(TestOnvifNotificationPayload TestOnvifNotificationPayload.cpp)
add_onvif_test(TestOnvifTlsSecurity TestOnvifTlsSecurity.cpp)
add_onvif_test(TestOnvifConcurrency TestOnvifConcurrency.cpp)
add_onvif_test(TestOnvifEventLifecycle TestOnvifEventLifecycle.cpp)
add_onvif_test(TestOnvifPtzConformance TestOnvifPtzConformance.cpp)
add_onvif_test(TestOnvifDiscoveryProfileS TestOnvifDiscoveryProfileS.cpp)
add_onvif_test(TestOnvifDeviceProfileS TestOnvifDeviceProfileS.cpp)
add_onvif_test(TestOnvifMediaProfileS TestOnvifMediaProfileS.cpp)
add_onvif_test(TestAsciiUtils TestAsciiUtils.cpp)
add_onvif_test(TestOnvifSnapshot TestOnvifSnapshot.cpp)
add_onvif_test(TestOnvifPtzProfileS TestOnvifPtzProfileS.cpp)
add_onvif_test(TestOnvifEventsProfileS TestOnvifEventsProfileS.cpp)
add_onvif_test(TestOnvifClientProfileS TestOnvifClientProfileS.cpp)
add_onvif_test(TestOnvifCryptoRaII TestOnvifCryptoRaII.cpp)
add_onvif_test(TestOnvifScopeStore TestOnvifScopeStore.cpp)
add_onvif_test(TestOnvifSoapRouter TestOnvifSoapRouter.cpp)
