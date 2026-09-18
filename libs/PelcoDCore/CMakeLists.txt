add_library(PelcoDCore STATIC
    PelcoDTypes.h
    RetryPolicy.h
    Connection.h
    DeviceStatus.h
    ITransport.h
    BaseTransport.h
    BaseTransport.cpp
    PelcoDFrame.h
    PelcoDFrame.cpp
    SerialTransport.h
    SerialTransport.cpp
    SocketUtils.h
    SocketUtils.cpp
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
    RttProfiler.h
    RttProfiler.cpp
    KinematicsSimulator.h
    KinematicsSimulator.cpp
    LatencyPipeline.h
    LatencyPipeline.cpp
    FujinonTypes.h
    FujinonBuilder.h
    FujinonBuilder.cpp
    FujinonParser.h
    FujinonParser.cpp
    FujinonSX800Device.h
    FujinonSX800Device.cpp
    PidController.h
    PidController.cpp
    PtzAutoTracker.h
    PtzAutoTracker.cpp
    Fft.h
    Fft.cpp
    NotchFilter.h
    NotchFilter.cpp
    OscillationDetector.h
    OscillationDetector.cpp
    GoertzelFilter.h
    GoertzelFilter.cpp
    LatencyEstimator.h
    LatencyEstimator.cpp
    Dct.h
    Dct.cpp
    DctSharpnessEvaluator.h
    DctSharpnessEvaluator.cpp
    PhaseCorrelation.h
    PhaseCorrelation.cpp
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
