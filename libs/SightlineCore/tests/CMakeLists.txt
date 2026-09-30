# ==============================================================================
# SightlineCore Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_sightline_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/Transport
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        SightlineCore
        Transport
        PelcoDTestSupport
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_sightline_test(TestSightlineCrc8        TestSightlineCrc8.cpp)
add_sightline_test(TestSightlineAccumulator TestSightlineAccumulator.cpp)
add_sightline_test(TestSightlineMessages    TestSightlineMessages.cpp)
add_sightline_test(TestSightlineDevice      TestSightlineDevice.cpp)
