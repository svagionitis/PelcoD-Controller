add_library(PelcoDQt STATIC
    QPelcoDDevice.h
    QPelcoDDevice.cpp
    QPatrolController.h
    QPatrolController.cpp
    QBusScanner.h
    QBusScanner.cpp
    QRttProfiler.h
    QRttProfiler.cpp
    QFujinonSX800Device.h
    QFujinonSX800Device.cpp
)

target_include_directories(PelcoDQt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDQt PUBLIC
    PelcoDCore
    Transport
    PelcoDFujinon
    Qt6::Core
)

if(PELCOD_ENABLE_VIDEO)
    target_sources(PelcoDQt PRIVATE
        QVideoStreamWorker.h
        QVideoStreamWorker.cpp
    )
    target_link_libraries(PelcoDQt PUBLIC
        Video
        Qt6::Gui
    )
endif()

if(PELCOD_ENABLE_ONVIF)
    target_sources(PelcoDQt PRIVATE
        QOnvifDevice.h
        QOnvifDevice.cpp
        QOnvifServer.h
        QOnvifServer.cpp
    )
    target_link_libraries(PelcoDQt PUBLIC
        Onvif
    )
    target_compile_definitions(PelcoDQt PUBLIC PELCOD_ENABLE_ONVIF=1)
endif()

apply_compiler_flags(PelcoDQt)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
