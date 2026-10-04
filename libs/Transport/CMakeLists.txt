add_library(Transport STATIC
    ITransport.h
    TransportTypes.h
    TransportStats.h
    BaseTransport.h
    BaseTransport.cpp
    SerialTransport.h
    SerialTransport.cpp
    TcpTransport.h
    TcpTransport.cpp
    UdpTransport.h
    UdpTransport.cpp
    SightlineUdpTransport.h
    SightlineUdpTransport.cpp
    SocketUtils.h
    SocketUtils.cpp
    TsNetworkTypes.h
    MpegTsNetworkPublisher.h
    MpegTsNetworkPublisher.cpp
)

target_include_directories(Transport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Transport PUBLIC
    Threads::Threads
    glog::glog
)

if(WIN32)
    target_link_libraries(Transport PUBLIC ws2_32)
endif()

apply_compiler_flags(Transport)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
