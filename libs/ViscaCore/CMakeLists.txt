add_library(ViscaCore STATIC
    ViscaTypes.h
    ViscaFrame.h
    ViscaFrame.cpp
    ViscaRxAccumulator.h
    ViscaRxAccumulator.cpp
    ViscaBuilder.h
    ViscaBuilder.cpp
    ViscaParser.h
    ViscaParser.cpp
    ViscaDevice.h
    ViscaDevice.cpp
    ViscaBusScanner.h
    ViscaBusScanner.cpp
)

target_include_directories(ViscaCore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(ViscaCore PUBLIC
    Transport
    Threads::Threads
)

apply_compiler_flags(ViscaCore)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
