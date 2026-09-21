# ==============================================================================
# PelcoDTransport Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_transport_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDTransport
        PelcoDCore
        Threads::Threads
    )
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_transport_test(TestTcpTransportTimeout TestTcpTransportTimeout.cpp)
add_transport_test(TestSerialTransport     TestSerialTransport.cpp)
add_transport_test(TestUdpTransport        TestUdpTransport.cpp)
