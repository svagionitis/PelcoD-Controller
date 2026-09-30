add_library(SightlineCore STATIC
    SightlineTypes.h
    SightlineCrc8.h
    SightlineCrc8.cpp
    SightlineMessages.h
    SightlineStreamAccumulator.h
    SightlineStreamAccumulator.cpp
    SightlineProtocolBuilder.h
    SightlineProtocolBuilder.cpp
    SightlineProtocolParser.h
    SightlineProtocolParser.cpp
    SightlineDevice.h
    SightlineDevice.cpp
)

target_include_directories(SightlineCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(SightlineCore PUBLIC
    Transport
    Threads::Threads
)

apply_compiler_flags(SightlineCore)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
