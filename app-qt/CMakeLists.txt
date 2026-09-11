add_executable(PelcoDApp
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
    resources.qrc
)

target_include_directories(PelcoDApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(PelcoDApp PRIVATE
    PelcoDQt
    Qt6::Widgets
)

apply_compiler_flags(PelcoDApp)
