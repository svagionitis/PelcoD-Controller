# ==============================================================================
# HTTP Server Library (cpp-httplib) Discovery
# Check system first (e.g. libcpp-httplib-dev on Debian/Ubuntu), fallback to FetchContent
# ==============================================================================
find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(httplib QUIET cpp-httplib)
    endif()
endif()

if(TARGET httplib::httplib)
    set(HTTPLIB_TARGET httplib::httplib)
elseif(httplib_FOUND)
    add_library(httplib_imported INTERFACE IMPORTED)
    target_include_directories(httplib_imported INTERFACE ${httplib_INCLUDE_DIRS})
    target_link_libraries(httplib_imported INTERFACE ${httplib_LIBRARIES})
    target_compile_definitions(httplib_imported INTERFACE ${httplib_CFLAGS_OTHER})
    set(HTTPLIB_TARGET httplib_imported)
else()
    include(FetchContent)
    FetchContent_Declare(
        httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.18.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(httplib)
    set(HTTPLIB_TARGET httplib::httplib)
endif()

add_library(Onvif STATIC
    Onvif.h
    OnvifTypes.h
    OnvifSecurity.h
    OnvifSecurity.cpp
    OnvifHttpClient.h
    OnvifHttpClient.cpp
    OnvifDiscovery.h
    OnvifDiscovery.cpp
    OnvifClient.h
    OnvifClient.cpp
    XmlUtils.h
    XmlUtils.cpp
    OnvifServerTypes.h
    WsDiscoveryCommon.h
    WsDiscoveryCommon.cpp
    WsDiscoveryServer.h
    WsDiscoveryServer.cpp
    OnvifServer.h
    OnvifServer.cpp
    GeodesyUtils.h
    adapters/PelcoDPtzAdapter.h
    adapters/PelcoDPtzAdapter.cpp
)

target_include_directories(Onvif PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/adapters>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Onvif PUBLIC
    CURL::libcurl
    pugixml::pugixml
    OpenSSL::Crypto
    Threads::Threads
    ${HTTPLIB_TARGET}
    PelcoDCore
    Transport
)

if(WIN32)
    target_link_libraries(Onvif PUBLIC ws2_32)
endif()

apply_compiler_flags(Onvif)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
