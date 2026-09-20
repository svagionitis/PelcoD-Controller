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
    widgets/BodePlotWidget.h
    widgets/BodePlotWidget.cpp
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
    dialogs/TacticalOverlaysDialog.h
    dialogs/TacticalOverlaysDialog.cpp
    dialogs/MacroPlaybackDialog.h
    dialogs/MacroPlaybackDialog.cpp
    resources.qrc
)

target_include_directories(PelcoDAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideo
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideoFilters
    ${CMAKE_SOURCE_DIR}/libs/PelcoDTracking
    ${CMAKE_SOURCE_DIR}/libs/PelcoDFujinon
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
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
    PelcoDFujinon
    PelcoDSim
    PelcoDTracking
    PelcoDVideo
    Qt6::Widgets
)

if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    target_link_libraries(PelcoDAppQt PRIVATE PelcoDVideoFilters)
endif()

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
