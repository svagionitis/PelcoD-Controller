add_library(PelcoDCore STATIC
    PelcoDTypes.h
    RetryPolicy.h
    Connection.h
    DeviceStatus.h
    ITransport.h
    PelcoDFrame.h
    PelcoDFrame.cpp
    ProtocolBuilder.h
    ProtocolBuilder.cpp
    ProtocolParser.h
    ProtocolParser.cpp
    PelcoDDevice.h
    PelcoDDevice.cpp
    PatrolController.h
    PatrolController.cpp
    BusScanner.h
    BusScanner.cpp
    RttProfiler.h
    RttProfiler.cpp
    MacroScript.h
    MacroScript.cpp
    MacroPlayer.h
    MacroPlayer.cpp
)

target_include_directories(PelcoDCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDCore PUBLIC
    Threads::Threads
    glog::glog
)

apply_compiler_flags(PelcoDCore)
