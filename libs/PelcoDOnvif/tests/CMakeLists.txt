# ==============================================================================
# PelcoDOnvif Unit Tests  (requires PELCOD_ENABLE_ONVIF)
# ==============================================================================

function(add_onvif_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDOnvif
        PelcoDCore
        PelcoDSim
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_onvif_test(TestPelcoDOnvif  TestPelcoDOnvif.cpp)
add_onvif_test(TestOnvifServer  TestOnvifServer.cpp)
