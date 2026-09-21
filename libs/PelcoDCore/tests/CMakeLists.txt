# ==============================================================================
# PelcoDCore Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_core_test test_name test_source)
    cmake_parse_arguments(ARG "GTEST" "" "" ${ARGN})
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
        ${CMAKE_SOURCE_DIR}/libs/PelcoDTransport
        ${CMAKE_SOURCE_DIR}/libs/PelcoDFujinon
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDCore
        PelcoDSim
        PelcoDTransport
        PelcoDFujinon
        PelcoDTestSupport
        Threads::Threads
    )
    apply_compiler_flags(${test_name})

    if(ARG_GTEST)
        target_link_libraries(${test_name} PRIVATE GTest::gmock_main)
        if(WIN32)
            if(TARGET Qt6::Core)
                add_custom_command(TARGET ${test_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::Core>"
                        "$<TARGET_FILE_DIR:${test_name}>"
                    COMMENT "Copying Qt6 Core DLL for ${test_name}"
                )
            endif()
            if(TARGET glog::glog)
                add_custom_command(TARGET ${test_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:glog::glog>"
                        "$<TARGET_FILE_DIR:${test_name}>"
                    COMMENT "Copying glog DLL for ${test_name}"
                )
            endif()
            if(TARGET gflags_shared)
                add_custom_command(TARGET ${test_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:gflags_shared>"
                        "$<TARGET_FILE_DIR:${test_name}>"
                    COMMENT "Copying gflags DLL for ${test_name}"
                )
            endif()
            gtest_discover_tests(${test_name}
                DISCOVERY_MODE PRE_TEST
                PROPERTIES ENVIRONMENT "PATH=C:/Qt/6.6.1/msvc2019_64/bin\;C:/vcpkg/installed/x64-windows/debug/bin\;C:/vcpkg/installed/x64-windows/bin\;$ENV{PATH}"
            )
        else()
            gtest_discover_tests(${test_name}
                DISCOVERY_MODE PRE_TEST
            )
        endif()
    else()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endfunction()

add_core_test(TestPelcoDFrame         TestPelcoDFrame.cpp         GTEST)
add_core_test(TestProtocolBuilder     TestProtocolBuilder.cpp     GTEST)
add_core_test(TestProtocolParser      TestProtocolParser.cpp      GTEST)
add_core_test(TestProtocolCompleteness TestProtocolCompleteness.cpp)
add_core_test(TestCommandQueue        TestCommandQueue.cpp)
add_core_test(TestStreamAccumulator   TestStreamAccumulator.cpp)
add_core_test(TestMacroPlayback       TestMacroPlayback.cpp)
add_core_test(TestPatrolController    TestPatrolController.cpp)
add_core_test(TestBusScanner          TestBusScanner.cpp)
add_core_test(TestRttProfiler         TestRttProfiler.cpp)
add_core_test(TestMockDevice          TestMockDevice.cpp)
add_core_test(TestConnection          TestConnection.cpp          GTEST)
