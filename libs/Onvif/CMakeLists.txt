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
    HttplibInclude.h
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
    WsDiscoveryValidator.h
    WsDiscoveryValidator.cpp
    ScopeStore.h
    ScopeStore.cpp
    OnvifServer.h
    OnvifServerImpl.h
    OnvifServer.cpp
    OnvifServerRoutes.cpp
    OnvifServerAuth.cpp
    ServiceRegistry.h

    ServiceRegistry.cpp
    OnvifAuthTypes.h
    OpenSslRaII.h
    OnvifCrypto.h
    OnvifCrypto.cpp
    NonceCache.h
    NonceCache.cpp
    CredentialStore.h
    CredentialStore.cpp
    PasswordPolicy.h
    PasswordPolicy.cpp
    CredentialVault.h
    CredentialVault.cpp
    UsernameToken.h
    UsernameToken.cpp
    HttpDigest.h
    HttpDigest.cpp
    AccessPolicy.h
    AccessPolicy.cpp
    Authenticator.h
    Authenticator.cpp
    SoapFault.h
    SoapFault.cpp
    SoapRouter.h
    SoapRouter.cpp
    NotificationUrlValidator.h
    NotificationUrlValidator.cpp
    NotificationDispatcher.h
    NotificationDispatcher.cpp
    SubscriptionManager.h
    SubscriptionManager.cpp
    JpegGenerator.h
    JpegGenerator.cpp
    AsciiUtils.h
    GeodesyUtils.h
    adapters/PelcoDPtzAdapter.h
    adapters/PelcoDPtzAdapter.cpp
    services/IService.h
    services/builders/DeviceXmlBuilder.h
    services/builders/DeviceXmlBuilder.cpp
    services/builders/MediaXmlBuilder.h
    services/builders/MediaXmlBuilder.cpp
    services/builders/PtzXmlBuilder.h
    services/builders/PtzXmlBuilder.cpp
    services/DeviceService.h
    services/DeviceService.cpp
    services/DeviceServiceNet.cpp
    services/MediaService.h
    services/MediaService.cpp
    services/MediaServiceExt.cpp
    services/MediaServiceConfigs.cpp
    services/PtzService.h
    services/PtzService.cpp
    services/PtzServiceTours.cpp
    services/ImagingService.h
    services/ImagingService.cpp
    services/EventService.h
    services/EventService.cpp
    services/AnalyticsService.h
    services/AnalyticsService.cpp
    services/DeviceIoService.h
    services/DeviceIoService.cpp
    services/ThermalService.h
    services/ThermalService.cpp
    services/RecordingService.h
    services/RecordingService.cpp
    services/RecordingServiceSearch.cpp
)

target_include_directories(Onvif PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/adapters>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/services>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/services/builders>
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
