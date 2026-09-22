# ==============================================================================
# Pure C++17 Terminal User Interface (Zero-dependency beyond POSIX termios)
# ==============================================================================
add_executable(PelcoDAppTui
    main.cpp
    Terminal.h
    Terminal.cpp
    Color.h
    Canvas.h
    Canvas.cpp
    UtfSymbols.h
    TuiApp.h
    TuiApp.cpp
    views/HeaderView.h
    views/HeaderView.cpp
    views/PtzView.h
    views/PtzView.cpp
    views/PresetsView.h
    views/PresetsView.cpp
    views/SettingsView.h
    views/SettingsView.cpp
    views/AuxOsdView.h
    views/AuxOsdView.cpp
    views/DiagnosticsView.h
    views/DiagnosticsView.cpp
    views/TrafficView.h
    views/TrafficView.cpp
    views/FujinonView.h
    views/FujinonView.cpp
    views/FooterView.h
    views/FooterView.cpp
    views/ConnectionModal.h
    views/ConnectionModal.cpp
    views/VideoView.h
    views/VideoView.cpp
)

target_include_directories(PelcoDAppTui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideo
    ${CMAKE_SOURCE_DIR}/libs/PelcoDVideoFilters
    ${CMAKE_SOURCE_DIR}/libs/PelcoDTracking
    ${CMAKE_SOURCE_DIR}/libs/PelcoDFujinon
    ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
)

target_link_libraries(PelcoDAppTui PRIVATE
    PelcoDCore
    PelcoDTransport
    PelcoDSim
    PelcoDTracking
    PelcoDFujinon
    PelcoDVideo
    Threads::Threads
)

if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    target_link_libraries(PelcoDAppTui PRIVATE PelcoDVideoFilters)
endif()

if(PELCOD_ENABLE_ONVIF)
    target_sources(PelcoDAppTui PRIVATE
        views/OnvifServerView.h
        views/OnvifServerView.cpp
    )
    target_link_libraries(PelcoDAppTui PRIVATE
        PelcoDOnvif
    )
    target_compile_definitions(PelcoDAppTui PRIVATE PELCOD_ENABLE_ONVIF=1)
endif()

apply_compiler_flags(PelcoDAppTui)
