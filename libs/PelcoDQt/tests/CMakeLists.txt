# ==============================================================================
# PelcoDQt Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_qt_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
        ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDQt
        PelcoDCore
        PelcoDSim
        Qt6::Core
        Qt6::Test
    )
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_qt_test(TestQPelcoDDevice TestQPelcoDDevice.cpp)
add_qt_test(TestRetryPolicy   TestRetryPolicy.cpp)

if(WIN32)
    foreach(qt_test IN ITEMS TestQPelcoDDevice TestRetryPolicy)
        set_property(TEST ${qt_test} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/bin"
        )
    endforeach()
endif()
