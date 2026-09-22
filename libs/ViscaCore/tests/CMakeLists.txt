# ==============================================================================
# ViscaCore Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_visca_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/Transport
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        ViscaCore
        Transport
        PelcoDTestSupport
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_visca_test(TestViscaFrame         TestViscaFrame.cpp)
add_visca_test(TestViscaRxAccumulator TestViscaRxAccumulator.cpp)
add_visca_test(TestViscaBuilder       TestViscaBuilder.cpp)
add_visca_test(TestViscaParser        TestViscaParser.cpp)
add_visca_test(TestViscaDevice        TestViscaDevice.cpp)
add_visca_test(TestViscaBusScanner    TestViscaBusScanner.cpp)
