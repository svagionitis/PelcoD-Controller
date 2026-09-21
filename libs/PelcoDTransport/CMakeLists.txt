add_library(PelcoDTransport STATIC
    BaseTransport.h
    BaseTransport.cpp
    SerialTransport.h
    SerialTransport.cpp
    TcpTransport.h
    TcpTransport.cpp
    UdpTransport.h
    UdpTransport.cpp
    SocketUtils.h
    SocketUtils.cpp
)

target_include_directories(PelcoDTransport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDTransport PUBLIC
    PelcoDCore
    Threads::Threads
    glog::glog
)

if(WIN32)
    target_link_libraries(PelcoDTransport PUBLIC ws2_32)
endif()

apply_compiler_flags(PelcoDTransport)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
