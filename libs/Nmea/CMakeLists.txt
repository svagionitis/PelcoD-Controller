add_library(Nmea STATIC
    NmeaTypes.h
    NmeaChecksum.h
    NmeaSentenceParser.h
    NmeaSentenceParser.cpp
    NmeaSentenceBuilder.h
    NmeaSentenceBuilder.cpp
    NmeaStreamAccumulator.h
    NmeaStreamAccumulator.cpp
    NmeaTagBlockParser.h
    NmeaTagBlockParser.cpp
    NmeaTagBlockBuilder.h
    NmeaTagBlockBuilder.cpp
    AisTypes.h
    AisBitReader.h
    AisDecoder.h
    AisDecoder.cpp
    PfecTypes.h
    NmeaDevice.h
    NmeaDevice.cpp
    FlirPfecDevice.h
    FlirPfecDevice.cpp
    n2k/N2kTypes.h
    n2k/N2kFastPacketAssembler.h
    n2k/N2kFastPacketAssembler.cpp
    n2k/N2kDecoder.h
    n2k/N2kDecoder.cpp
    n2k/N2kDevice.h
    n2k/N2kDevice.cpp
    lwe/LweTypes.h
    lwe/LweMulticastTransport.h
    lwe/LweMulticastTransport.cpp
    lwe/LweChannelManager.h
    lwe/LweChannelManager.cpp
    replay/ReplayTypes.h
    replay/NmeaReplayTransport.h
    replay/NmeaReplayTransport.cpp
    route/NmeaRouteManager.h
    route/NmeaRouteManager.cpp
    environment/ThermalTuningAdvisor.h
    environment/ThermalTuningAdvisor.cpp
)

target_include_directories(Nmea PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/n2k>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lwe>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/replay>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/route>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/environment>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Nmea PUBLIC
    Transport
    Threads::Threads
    glog::glog
)

apply_compiler_flags(Nmea)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
