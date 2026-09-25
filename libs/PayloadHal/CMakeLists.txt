add_library(PayloadHal STATIC
    PayloadTypes.h
    IDevice.h
    IPanTiltUnit.h
    ILaserRangeFinder.h
    ICameraPayload.h
    IPayload.h
    GeoreferenceUtils.h
    GeoreferenceUtils.cpp
    PayloadHal.h
    PayloadFactory.h
    PayloadFactory.cpp
    adapters/PelcoDPtzAdapter.h
    adapters/PelcoDPtzAdapter.cpp
    adapters/PelcoDFujinonPayloadAdapter.h
    adapters/PelcoDFujinonPayloadAdapter.cpp
    adapters/ViscaSonyAdapter.h
    adapters/ViscaSonyAdapter.cpp
    adapters/PelcoDViscaCompositePayload.h
    adapters/PelcoDViscaCompositePayload.cpp
    adapters/OnvifPayloadAdapter.h
    adapters/OnvifPayloadAdapter.cpp
    sim/SimulatedPayload.h
    sim/SimulatedPayload.cpp
)

target_include_directories(PayloadHal PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../ViscaSony>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Onvif>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PayloadHal PUBLIC
    Klv
    Math
    PelcoDCore
    PelcoDFujinon
    ViscaSony
    Onvif
    Transport
)

apply_compiler_flags(PayloadHal)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
