add_library(MappingQt STATIC
    TacticalMapQuickItem.h
    TacticalMapQuickItem.cpp
)

target_include_directories(MappingQt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(MappingQt PUBLIC
    Mapping
    Klv
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Threads::Threads
    glog::glog
)

apply_compiler_flags(MappingQt)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
