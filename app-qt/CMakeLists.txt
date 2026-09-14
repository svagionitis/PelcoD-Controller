add_executable(PelcoDAppQt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    widgets/ConnectionWidget.h
    widgets/ConnectionWidget.cpp
    widgets/TrafficInspectorWidget.h
    widgets/TrafficInspectorWidget.cpp
    tabs/PtzControlTab.h
    tabs/PtzControlTab.cpp
    tabs/PresetsTab.h
    tabs/PresetsTab.cpp
    tabs/DeviceSettingsTab.h
    tabs/DeviceSettingsTab.cpp
    tabs/AuxZonesTab.h
    tabs/AuxZonesTab.cpp
    tabs/OsdScreenTab.h
    tabs/OsdScreenTab.cpp
    tabs/SystemTab.h
    tabs/SystemTab.cpp
    tabs/FujinonSX800Tab.h
    tabs/FujinonSX800Tab.cpp
    dialogs/BusScanDialog.h
    dialogs/BusScanDialog.cpp
    dialogs/MockDynamicsDialog.h
    dialogs/MockDynamicsDialog.cpp
    resources.qrc
)

target_include_directories(PelcoDAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(PelcoDAppQt PRIVATE
    PelcoDQt
    Qt6::Widgets
)

apply_compiler_flags(PelcoDAppQt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET PelcoDAppQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:PelcoDAppQt>"
        COMMENT "Deploying Qt libraries for PelcoDAppQt using windeployqt..."
    )
endif()
