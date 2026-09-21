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
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_onvif_test(TestPelcoDOnvif  TestPelcoDOnvif.cpp)
add_onvif_test(TestOnvifServer  TestOnvifServer.cpp)

if(WIN32)
    foreach(onvif_test IN ITEMS TestPelcoDOnvif TestOnvifServer)
        set_property(TEST ${onvif_test} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/bin"
        )
    endforeach()
endif()
