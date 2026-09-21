# ==============================================================================
# PelcoDVideo Unit Tests  (requires PELCOD_ENABLE_VIDEO)
# ==============================================================================

add_executable(TestVideoDecoder TestVideoDecoder.cpp)
target_include_directories(TestVideoDecoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideoFilters
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
)
target_link_libraries(TestVideoDecoder PRIVATE
    PelcoDQt
    PelcoDVideo
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    target_link_libraries(TestVideoDecoder PRIVATE PelcoDVideoFilters)
endif()
apply_compiler_flags(TestVideoDecoder)
add_test(NAME TestVideoDecoder COMMAND TestVideoDecoder)

if(WIN32)
    set_property(TEST TestVideoDecoder PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
        "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/debug/bin"
        "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/bin"
    )
endif()
