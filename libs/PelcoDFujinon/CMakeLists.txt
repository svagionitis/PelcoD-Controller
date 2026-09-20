add_library(PelcoDFujinon STATIC
    FujinonTypes.h
    FujinonBuilder.h
    FujinonBuilder.cpp
    FujinonParser.h
    FujinonParser.cpp
    FujinonSX800Device.h
    FujinonSX800Device.cpp
    PelcoDFujinon.h
)

target_include_directories(PelcoDFujinon PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDFujinon PUBLIC
    PelcoDCore
    Threads::Threads
    glog::glog
)

apply_compiler_flags(PelcoDFujinon)
