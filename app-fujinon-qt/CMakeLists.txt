add_executable(FujinonAppQt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    widgets/ConnectionWidget.h
    widgets/ConnectionWidget.cpp
    widgets/TrafficInspectorWidget.h
    widgets/TrafficInspectorWidget.cpp
    tabs/FujinonSX800Tab.h
    tabs/FujinonSX800Tab.cpp
    dialogs/BusScanDialog.h
    dialogs/BusScanDialog.cpp
    dialogs/MockDynamicsDialog.h
    dialogs/MockDynamicsDialog.cpp
    dialogs/RttProfilerDialog.h
    dialogs/RttProfilerDialog.cpp
    dialogs/TacticalOverlaysDialog.h
    dialogs/TacticalOverlaysDialog.cpp
    dialogs/MacroPlaybackDialog.h
    dialogs/MacroPlaybackDialog.cpp
    resources.qrc
)

target_include_directories(FujinonAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/PelcoDFujinon
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
)

target_link_libraries(FujinonAppQt PRIVATE
    PelcoDQt
    PelcoDFujinon
    PelcoDSim
    Transport
    Qt6::Widgets
)

apply_compiler_flags(FujinonAppQt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET FujinonAppQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:FujinonAppQt>"
        COMMENT "Deploying Qt libraries for FujinonAppQt using windeployqt..."
    )
endif()
