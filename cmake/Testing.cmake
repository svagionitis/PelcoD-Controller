# ==============================================================================
# Testing & Code Coverage Configuration
# ==============================================================================

if(PELCOD_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "PelcoD: Code coverage enabled (--coverage -O0 -g)")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(STATUS "PelcoD: PELCOD_ENABLE_COVERAGE active on MSVC. Use OpenCppCoverage for execution analysis.")
    endif()
endif()

enable_testing()

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    message(STATUS "PelcoD: GTest not found in system/vcpkg. Fetching GoogleTest via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(_PELCOD_SAVED_AUTOUIC ${CMAKE_AUTOUIC})
    set(_PELCOD_SAVED_AUTOMOC ${CMAKE_AUTOMOC})
    set(_PELCOD_SAVED_AUTORCC ${CMAKE_AUTORCC})
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTORCC OFF)

    FetchContent_MakeAvailable(googletest)

    set(CMAKE_AUTOUIC ${_PELCOD_SAVED_AUTOUIC})
    set(CMAKE_AUTOMOC ${_PELCOD_SAVED_AUTOMOC})
    set(CMAKE_AUTORCC ${_PELCOD_SAVED_AUTORCC})

    if(TARGET gtest)
        set_target_properties(gtest PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endif()
    if(TARGET gtest_main)
        set_target_properties(gtest_main PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endif()
    if(TARGET gmock)
        set_target_properties(gmock PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endif()
    if(TARGET gmock_main)
        set_target_properties(gmock_main PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    endif()
endif()
include(GoogleTest)

# Determine Qt and vcpkg runtime paths for test execution and discovery
if(WIN32)
    if(TARGET Qt6::Core)
        get_filename_component(_PELCOD_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
        get_filename_component(_PELCOD_QT_PLUGINS_DIR "${Qt6_DIR}/../../../plugins" ABSOLUTE)
    else()
        set(_PELCOD_QT_BIN_DIR "")
        set(_PELCOD_QT_PLUGINS_DIR "")
    endif()

    set(_PELCOD_VCPKG_BIN_DIRS "")
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        list(APPEND _PELCOD_VCPKG_BIN_DIRS
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        )
    endif()
    list(APPEND _PELCOD_VCPKG_BIN_DIRS
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin"
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
        "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
        "C:/vcpkg/installed/x64-windows/debug/bin"
        "C:/vcpkg/installed/x64-windows/bin"
    )
    list(REMOVE_DUPLICATES _PELCOD_VCPKG_BIN_DIRS)

    find_file(_GFLAGS_DEBUG_DLL NAMES gflags_debug.dll gflags.dll
        PATHS ${_PELCOD_VCPKG_BIN_DIRS}
    )
endif()

function(pelcod_register_gtest target_name)
    cmake_parse_arguments(ARG "NO_MAIN" "" "" ${ARGN})
    if(ARG_NO_MAIN)
        target_link_libraries(${target_name} PRIVATE GTest::gmock)
    else()
        target_link_libraries(${target_name} PRIVATE GTest::gmock_main)
    endif()
    apply_compiler_flags(${target_name})

    if(WIN32)
        if(TARGET Qt6::Core)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::Core>"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying Qt6 Core DLL for ${target_name}"
            )
        endif()
        if(TARGET Qt6::Gui)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::Gui>"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying Qt6 Gui DLL for ${target_name}"
            )
        endif()
        if(TARGET Qt6::Widgets)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Qt6::Widgets>"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying Qt6 Widgets DLL for ${target_name}"
            )
        endif()
        if(TARGET glog::glog)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:glog::glog>"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying glog DLL for ${target_name}"
            )
        endif()
        if(_GFLAGS_DEBUG_DLL)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_GFLAGS_DEBUG_DLL}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                COMMENT "Copying gflags DLL for ${target_name}"
            )
        endif()
    endif()

    set(_ENV_SETUP_BLOCK "set(ENV{QT_QPA_PLATFORM} \"offscreen\")\n")
    if(WIN32)
        string(JOIN ";" _TARGET_SEARCH_PATHS
            "${_PELCOD_QT_BIN_DIR}"
            ${_PELCOD_VCPKG_BIN_DIRS}
            "$<TARGET_FILE_DIR:${target_name}>"
            "${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
            "${CMAKE_BINARY_DIR}/lib"
        )
        string(PREPEND _ENV_SETUP_BLOCK
            "set(ENV{PATH} \"${_TARGET_SEARCH_PATHS};\$ENV{PATH}\")\n"
            "if(NOT \"${_PELCOD_QT_PLUGINS_DIR}\" STREQUAL \"\")\n"
            "    set(ENV{QT_PLUGIN_PATH} \"${_PELCOD_QT_PLUGINS_DIR}\")\n"
            "endif()\n"
        )
    endif()

    set(_RUNNER_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/${target_name}_runner_$<CONFIG>.cmake")
    file(GENERATE OUTPUT "${_RUNNER_SCRIPT}" CONTENT
"# Auto-generated test runner for ${target_name}
${_ENV_SETUP_BLOCK}
set(cmd \"\")
set(found_delim FALSE)
math(EXPR last_idx \"\${CMAKE_ARGC} - 1\")
foreach(i RANGE 0 \${last_idx})
    set(arg \"\${CMAKE_ARGV\${i}}\")
    if(found_delim)
        list(APPEND cmd \"\${arg}\")
    elseif(arg STREQUAL \"--\")
        set(found_delim TRUE)
    endif()
endforeach()

execute_process(
    COMMAND \${cmd}
    RESULT_VARIABLE res
)
if(NOT res EQUAL 0)
    if(res MATCHES \"^[0-9]+$\")
        cmake_language(EXIT \${res})
    else()
        message(FATAL_ERROR \"Execution failed: \${res}\")
    endif()
endif()
")

    set_target_properties(${target_name} PROPERTIES
        TEST_LAUNCHER "${CMAKE_COMMAND};-P;${_RUNNER_SCRIPT};--"
    )

    gtest_discover_tests(${target_name}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

if(WIN32)
    find_program(OPENCPPCOVERAGE_EXE NAMES OpenCppCoverage.exe PATHS "C:/Program Files/OpenCppCoverage")
    if(OPENCPPCOVERAGE_EXE)
        find_program(POWERSHELL_EXE NAMES pwsh powershell)
        if(NOT POWERSHELL_EXE)
            set(POWERSHELL_EXE powershell)
        endif()
        add_custom_target(coverage
            COMMAND "${POWERSHELL_EXE}" -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/scripts/RunCoverage.ps1"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            COMMENT "Generating unified code coverage report with OpenCppCoverage..."
        )
    endif()
endif()
