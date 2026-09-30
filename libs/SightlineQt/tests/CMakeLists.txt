# ==============================================================================
# SightlineQt Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_sightline_qt_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/SightlineCore
        ${CMAKE_SOURCE_DIR}/libs/Transport
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        SightlineQt
        SightlineCore
        Transport
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
    )
    pelcod_register_gtest(${test_name} NO_MAIN)
endfunction()

add_sightline_qt_test(TestQSightlineDevice TestQSightlineDevice.cpp)
