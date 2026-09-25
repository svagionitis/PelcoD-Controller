# ==============================================================================
# PayloadHal Unit Tests
# ==============================================================================

function(add_payload_hal_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../ViscaSony
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Onvif
    )
    target_link_libraries(${test_name} PRIVATE
        PayloadHal
        Klv
        Math
        PelcoDCore
        PelcoDFujinon
        PelcoDSim
        ViscaSony
        Onvif
        Transport
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_payload_hal_test(TestPayloadHalUnits          TestPayloadHalUnits.cpp)
add_payload_hal_test(TestTargetGeoreferencing     TestTargetGeoreferencing.cpp)
add_payload_hal_test(TestLrfSafetyInterlock       TestLrfSafetyInterlock.cpp)
add_payload_hal_test(TestPelcoDAdapter            TestPelcoDAdapter.cpp)
add_payload_hal_test(TestPelcoDFujinonAdapter     TestPelcoDFujinonAdapter.cpp)
add_payload_hal_test(TestViscaAdapter             TestViscaAdapter.cpp)
add_payload_hal_test(TestPelcoDViscaAdapter        TestPelcoDViscaAdapter.cpp)
add_payload_hal_test(TestSimulatedPayload         TestSimulatedPayload.cpp)
add_payload_hal_test(TestOnvifAdapter             TestOnvifAdapter.cpp)
add_payload_hal_test(TestPayloadFactoryUri        TestPayloadFactoryUri.cpp)

