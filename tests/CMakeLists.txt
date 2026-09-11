function(add_pelco_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE PelcoDCore)
    apply_compiler_flags(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_pelco_test(TestPelcoDFrame TestPelcoDFrame.cpp)
add_pelco_test(TestCircularByteRing TestCircularByteRing.cpp)
add_pelco_test(TestProtocolBuilder TestProtocolBuilder.cpp)
add_pelco_test(TestProtocolParser TestProtocolParser.cpp)
add_pelco_test(TestMockDevice TestMockDevice.cpp)
add_pelco_test(TestTcpTransportTimeout TestTcpTransportTimeout.cpp)
add_pelco_test(TestProtocolCompleteness TestProtocolCompleteness.cpp)

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
)
target_include_directories(TestAutoReconnect PRIVATE
    ${CMAKE_SOURCE_DIR}/app-qt
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
)
target_link_libraries(TestAutoReconnect PRIVATE
    PelcoDCore
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
apply_compiler_flags(TestAutoReconnect)
add_test(NAME TestAutoReconnect COMMAND TestAutoReconnect)

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

