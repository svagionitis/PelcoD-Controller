add_library(ViscaSony STATIC
    SonyViscaTypes.h
    SonyCameraModel.h
    SonyViscaBuilder.h
    SonyViscaBuilder.cpp
    SonyViscaParser.h
    SonyViscaParser.cpp
    SonyFCBDevice.h
    SonyFCBDevice.cpp
    MockSonyCamera.h
    MockSonyCamera.cpp
)

target_include_directories(ViscaSony PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ViscaSony PUBLIC
    ViscaCore
    Transport
    Threads::Threads
)

apply_compiler_flags(ViscaSony)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
