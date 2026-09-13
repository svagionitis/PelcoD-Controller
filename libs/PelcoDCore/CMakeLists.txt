add_library(PelcoDCore STATIC
    PelcoDTypes.h
    DeviceStatus.h
    ITransport.h
    BaseTransport.h
    BaseTransport.cpp
    PelcoDFrame.h
    PelcoDFrame.cpp
    CircularByteRing.h
    SerialTransport.h
    SerialTransport.cpp
    TcpTransport.h
    TcpTransport.cpp
    UdpTransport.h
    UdpTransport.cpp
    MockPelcoDDevice.h
    MockPelcoDDevice.cpp
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
    KinematicsSimulator.h
    KinematicsSimulator.cpp
    LatencyPipeline.h
    LatencyPipeline.cpp
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

if(WIN32)
    target_link_libraries(PelcoDCore PUBLIC ws2_32)
endif()

apply_compiler_flags(PelcoDCore)
