# ==============================================================================
# DjiTelemetry Unit Tests
# ==============================================================================

function(add_dji_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        DjiTelemetry
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_dji_test(TestDjiSubtitleParser   TestDjiSubtitleParser.cpp)
add_dji_test(TestMp4TextTrackReader  TestMp4TextTrackReader.cpp)
add_dji_test(TestDjiSt0601Mapper     TestDjiSt0601Mapper.cpp)
add_dji_test(TestDjiTelemetrySource  TestDjiTelemetrySource.cpp)
