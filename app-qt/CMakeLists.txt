add_executable(PelcoDAppQt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    widgets/ConnectionWidget.h
    widgets/ConnectionWidget.cpp
    widgets/TrafficInspectorWidget.h
    widgets/TrafficInspectorWidget.cpp
    widgets/VideoOverlayWidget.h
    widgets/VideoOverlayWidget.cpp
    widgets/SpectrogramWidget.h
    widgets/SpectrogramWidget.cpp
    tabs/VideoStreamTab.h
    tabs/VideoStreamTab.cpp
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
    dialogs/RttProfilerDialog.h
    dialogs/RttProfilerDialog.cpp
    resources.qrc
)

target_include_directories(PelcoDAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideo
)

if(PELCOD_ENABLE_ONVIF)
    target_sources(PelcoDAppQt PRIVATE
        tabs/OnvifCameraTab.h
        tabs/OnvifCameraTab.cpp
        tabs/OnvifServerTab.h
        tabs/OnvifServerTab.cpp
    )
    target_compile_definitions(PelcoDAppQt PRIVATE PELCOD_ENABLE_ONVIF=1)
endif()

target_link_libraries(PelcoDAppQt PRIVATE
    PelcoDQt
    PelcoDVideo
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
