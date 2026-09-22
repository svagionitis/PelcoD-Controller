# Ensure assert() remains active in test executables regardless of build configuration
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")

# ==============================================================================
# Application-level Tests (TUI Widget / Qt Widget / CLI Argument Validation)
# NOTE: All library-level unit tests now live under libs/<LibName>/tests/
# ==============================================================================

add_executable(TestConnectionModal
    TestConnectionModal.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/views/ConnectionModal.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/Canvas.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/Terminal.cpp
)
target_include_directories(TestConnectionModal PRIVATE
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
)
target_link_libraries(TestConnectionModal PRIVATE PelcoDCore Transport PelcoDSim Threads::Threads)
pelcod_register_gtest(TestConnectionModal)

add_executable(TestTrafficView
    TestTrafficView.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/views/TrafficView.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/Canvas.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui/Terminal.cpp
)
target_include_directories(TestTrafficView PRIVATE
    ${CMAKE_SOURCE_DIR}/app-pelcod-tui
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
)
target_link_libraries(TestTrafficView PRIVATE PelcoDCore Transport PelcoDSim Threads::Threads)
pelcod_register_gtest(TestTrafficView)

add_executable(TestAutoReconnect
    TestAutoReconnect.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-qt/widgets/ConnectionWidget.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-qt/dialogs/BusScanDialog.cpp
    ${CMAKE_SOURCE_DIR}/app-pelcod-qt/dialogs/MockDynamicsDialog.cpp
)
target_include_directories(TestAutoReconnect PRIVATE
    ${CMAKE_SOURCE_DIR}/app-pelcod-qt
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
)
target_link_libraries(TestAutoReconnect PRIVATE
    PelcoDQt
    PelcoDCore
    PelcoDSim
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
pelcod_register_gtest(TestAutoReconnect NO_MAIN)

add_executable(TestViscaAppQt
    TestViscaAppQt.cpp
    ${CMAKE_SOURCE_DIR}/app-visca-qt/QViscaSonyDevice.cpp
    ${CMAKE_SOURCE_DIR}/app-visca-qt/QViscaBusScanner.cpp
    ${CMAKE_SOURCE_DIR}/app-visca-qt/widgets/ViscaConnectionWidget.cpp
    ${CMAKE_SOURCE_DIR}/app-visca-qt/widgets/ViscaTrafficInspectorWidget.cpp
    ${CMAKE_SOURCE_DIR}/app-visca-qt/dialogs/ViscaBusScanDialog.cpp
)
target_include_directories(TestViscaAppQt PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/app-visca-qt
    ${CMAKE_SOURCE_DIR}/libs/ViscaCore
    ${CMAKE_SOURCE_DIR}/libs/ViscaSony
    ${CMAKE_SOURCE_DIR}/libs/Transport
)
target_link_libraries(TestViscaAppQt PRIVATE
    ViscaSony
    ViscaCore
    Transport
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    glog::glog
)
pelcod_register_gtest(TestViscaAppQt NO_MAIN)

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
