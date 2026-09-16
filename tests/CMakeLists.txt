# Ensure assert() remains active in test executables regardless of build configuration
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")

function(add_pelco_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE PelcoDCore)
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_pelco_test(TestPelcoDFrame TestPelcoDFrame.cpp)
add_pelco_test(TestProtocolBuilder TestProtocolBuilder.cpp)
add_pelco_test(TestProtocolParser TestProtocolParser.cpp)
add_pelco_test(TestMockDevice TestMockDevice.cpp)
add_pelco_test(TestTcpTransportTimeout TestTcpTransportTimeout.cpp)
add_pelco_test(TestProtocolCompleteness TestProtocolCompleteness.cpp)
add_pelco_test(TestSerialTransport TestSerialTransport.cpp)
add_pelco_test(TestUdpTransport TestUdpTransport.cpp)
add_pelco_test(TestPatrolController TestPatrolController.cpp)
add_pelco_test(TestBusScanner TestBusScanner.cpp)
add_pelco_test(TestMockKinematics TestMockKinematics.cpp)
add_pelco_test(TestFujinonProtocol TestFujinonProtocol.cpp)
add_pelco_test(TestFujinonSX800Device TestFujinonSX800Device.cpp)
add_pelco_test(TestConnection TestConnection.cpp)
add_pelco_test(TestRttProfiler TestRttProfiler.cpp)
add_pelco_test(TestPidController TestPidController.cpp)

add_executable(TestConnectionModal
    TestConnectionModal.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/views/ConnectionModal.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/Canvas.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/Terminal.cpp
)
target_include_directories(TestConnectionModal PRIVATE
    ${CMAKE_SOURCE_DIR}/app-tui
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
)
target_link_libraries(TestConnectionModal PRIVATE PelcoDCore Threads::Threads)
apply_compiler_flags(TestConnectionModal)
add_test(NAME TestConnectionModal COMMAND TestConnectionModal)

add_executable(TestTrafficView
    TestTrafficView.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/views/TrafficView.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/Canvas.cpp
    ${CMAKE_SOURCE_DIR}/app-tui/Terminal.cpp
)
target_include_directories(TestTrafficView PRIVATE
    ${CMAKE_SOURCE_DIR}/app-tui
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
)
target_link_libraries(TestTrafficView PRIVATE PelcoDCore Threads::Threads)
apply_compiler_flags(TestTrafficView)
add_test(NAME TestTrafficView COMMAND TestTrafficView)

add_executable(TestAutoReconnect
    TestAutoReconnect.cpp
    ${CMAKE_SOURCE_DIR}/app-qt/widgets/ConnectionWidget.cpp
    ${CMAKE_SOURCE_DIR}/app-qt/dialogs/BusScanDialog.cpp
    ${CMAKE_SOURCE_DIR}/app-qt/dialogs/MockDynamicsDialog.cpp
)
target_include_directories(TestAutoReconnect PRIVATE
    ${CMAKE_SOURCE_DIR}/app-qt
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
)
target_link_libraries(TestAutoReconnect PRIVATE
    PelcoDQt
    PelcoDCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
apply_compiler_flags(TestAutoReconnect)
add_test(NAME TestAutoReconnect COMMAND TestAutoReconnect)

add_executable(TestQPelcoDDevice
    TestQPelcoDDevice.cpp
)
target_include_directories(TestQPelcoDDevice PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
)
target_link_libraries(TestQPelcoDDevice PRIVATE
    PelcoDQt
    PelcoDCore
    Qt6::Core
    Qt6::Test
)
apply_compiler_flags(TestQPelcoDDevice)
add_test(NAME TestQPelcoDDevice COMMAND TestQPelcoDDevice)

add_executable(TestRetryPolicy
    TestRetryPolicy.cpp
)
target_include_directories(TestRetryPolicy PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
)
target_link_libraries(TestRetryPolicy PRIVATE
    PelcoDQt
    PelcoDCore
    Qt6::Core
    Qt6::Test
)
apply_compiler_flags(TestRetryPolicy)
add_test(NAME TestRetryPolicy COMMAND TestRetryPolicy)

if(WIN32)
    set(QT_DEVICE_TESTS
        TestAutoReconnect
        TestQPelcoDDevice
        TestRetryPolicy
    )
    foreach(qt_test IN LISTS QT_DEVICE_TESTS)
        set_property(TEST ${qt_test} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/bin"
        )
    endforeach()
endif()

# ==============================================================================
# CLI Argument Validation Tests (PelcoDAppTui)
# ==============================================================================
add_test(NAME TestTuiHelp COMMAND PelcoDAppTui --help)
add_test(NAME TestTuiInvalidAddress COMMAND PelcoDAppTui --address notanumber)
set_tests_properties(TestTuiInvalidAddress PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiAddressOutOfRange COMMAND PelcoDAppTui --address 300)
set_tests_properties(TestTuiAddressOutOfRange PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiInvalidTcpPort COMMAND PelcoDAppTui --tcp 127.0.0.1:99999)
set_tests_properties(TestTuiInvalidTcpPort PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiUnknownOption COMMAND PelcoDAppTui --unknown-flag)
set_tests_properties(TestTuiUnknownOption PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiMissingArg COMMAND PelcoDAppTui --tcp)
set_tests_properties(TestTuiMissingArg PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiInvalidUdpPort COMMAND PelcoDAppTui --udp 127.0.0.1:99999)
set_tests_properties(TestTuiInvalidUdpPort PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiMissingUdpArg COMMAND PelcoDAppTui --udp)
set_tests_properties(TestTuiMissingUdpArg PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiInvalidScanRange COMMAND PelcoDAppTui --scan 50-10)
set_tests_properties(TestTuiInvalidScanRange PROPERTIES WILL_FAIL TRUE)
add_test(NAME TestTuiScanMock COMMAND PelcoDAppTui --mock --scan 1-2)

if(WIN32)
    set(TUI_CLI_TESTS
        TestTuiHelp
        TestTuiInvalidAddress
        TestTuiAddressOutOfRange
        TestTuiInvalidTcpPort
        TestTuiUnknownOption
        TestTuiMissingArg
        TestTuiInvalidUdpPort
        TestTuiMissingUdpArg
        TestTuiInvalidScanRange
        TestTuiScanMock
    )
    foreach(tui_t IN LISTS TUI_CLI_TESTS)
        set_property(TEST ${tui_t} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/debug/bin"
            "PATH=path_list_prepend:C:/vcpkg/installed/x64-windows/bin"
        )
    endforeach()
endif()

if(PELCOD_ENABLE_VIDEO)
    add_executable(TestVideoDecoder
        TestVideoDecoder.cpp
    )
    target_include_directories(TestVideoDecoder PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/PelcoDVideo
        ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
    )
    target_link_libraries(TestVideoDecoder PRIVATE
        PelcoDQt
        PelcoDVideo
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
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
endif()
