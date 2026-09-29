add_library(Nmea STATIC
    NmeaTypes.h
    NmeaChecksum.h
    NmeaSentenceParser.h
    NmeaSentenceParser.cpp
    NmeaSentenceBuilder.h
    NmeaSentenceBuilder.cpp
    NmeaStreamAccumulator.h
    NmeaStreamAccumulator.cpp
)

target_include_directories(Nmea PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Nmea PUBLIC
    Threads::Threads
    glog::glog
)

apply_compiler_flags(Nmea)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
