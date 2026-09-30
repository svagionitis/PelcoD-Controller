add_library(SightlineQt STATIC
    QSightlineDevice.h
    QSightlineDevice.cpp
    SightlineTrafficInspectorWidget.h
    SightlineTrafficInspectorWidget.cpp
)

target_include_directories(SightlineQt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SightlineQt PUBLIC
    SightlineCore
    Transport
    Qt6::Core
    Qt6::Widgets
)

apply_compiler_flags(SightlineQt)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
