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
    Qt6::Core
)

if(PELCOD_ENABLE_VIDEO)
    target_sources(PelcoDQt PRIVATE
        QVideoStreamWorker.h
        QVideoStreamWorker.cpp
    )
    target_link_libraries(PelcoDQt PUBLIC
        PelcoDVideo
        Qt6::Gui
    )
endif()

apply_compiler_flags(PelcoDQt)
