# ==============================================================================
# Mapping Unit Tests
# ==============================================================================

function(add_mapping_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${test_name} PRIVATE
        Mapping
        Klv
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_mapping_test(TestMercatorProjection TestMercatorProjection.cpp)
add_mapping_test(TestMapViewport        TestMapViewport.cpp)
add_mapping_test(TestTileCache          TestTileCache.cpp)
add_mapping_test(TestOfflineTileStore   TestOfflineTileStore.cpp)
add_mapping_test(TestTacticalOverlay    TestTacticalOverlay.cpp)
