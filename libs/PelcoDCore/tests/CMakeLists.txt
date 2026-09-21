# ==============================================================================
# PelcoDCore Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_core_test test_name test_source)
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
        Threads::Threads
    )
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_core_test(TestPelcoDFrame         TestPelcoDFrame.cpp)
add_core_test(TestProtocolBuilder     TestProtocolBuilder.cpp)
add_core_test(TestProtocolParser      TestProtocolParser.cpp)
add_core_test(TestProtocolCompleteness TestProtocolCompleteness.cpp)
add_core_test(TestCommandQueue        TestCommandQueue.cpp)
add_core_test(TestStreamAccumulator   TestStreamAccumulator.cpp)
add_core_test(TestMacroPlayback       TestMacroPlayback.cpp)
add_core_test(TestPatrolController    TestPatrolController.cpp)
add_core_test(TestBusScanner          TestBusScanner.cpp)
add_core_test(TestRttProfiler         TestRttProfiler.cpp)
add_core_test(TestMockDevice          TestMockDevice.cpp)
add_core_test(TestConnection          TestConnection.cpp)
