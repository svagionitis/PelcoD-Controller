add_library(PelcoDSim STATIC
    KinematicsSimulator.h
    KinematicsSimulator.cpp
    LatencyPipeline.h
    LatencyPipeline.cpp
    MockPelcoDDevice.h
    MockPelcoDDevice.cpp
    PelcoDSim.h
)

target_include_directories(PelcoDSim PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDSim PUBLIC
    PelcoDCore
    Threads::Threads
    glog::glog
)

apply_compiler_flags(PelcoDSim)
