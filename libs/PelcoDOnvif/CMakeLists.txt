add_library(PelcoDOnvif STATIC
    OnvifTypes.h
    OnvifSecurity.h
    OnvifSecurity.cpp
    OnvifHttpClient.h
    OnvifHttpClient.cpp
    OnvifDiscovery.h
    OnvifDiscovery.cpp
    OnvifClient.h
    OnvifClient.cpp
)

target_include_directories(PelcoDOnvif PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDOnvif PUBLIC
    CURL::libcurl
    pugixml::pugixml
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(PelcoDOnvif PUBLIC ws2_32)
endif()

apply_compiler_flags(PelcoDOnvif)
