cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0100)
    cmake_policy(SET CMP0100 NEW)
endif()
# Disable Qt Creator automatic vcpkg and MaintenanceTool provider warnings
set(QT_CREATOR_SKIP_VCPKG_SETUP ON CACHE BOOL "Disable Qt Creator automatic vcpkg setup")
set(QT_CREATOR_ENABLE_MAINTENANCE_TOOL_PROVIDER OFF CACHE BOOL "Disable Qt Creator MaintenanceTool provider")

project(PelcoDController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable the compile_commands.json
# See https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# Compiler Flags, Security Hardening, and Sanitizers
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerFlags)

# ==============================================================================
# Application & GUI Options
# ==============================================================================
option(PELCOD_ENABLE_QT "Enable Qt 6 integration and GUI components" ON)
option(PELCOD_BUILD_APPS "Build end-user application binaries (GUI and TUI)" ON)

# ==============================================================================
# Dependencies
# ==============================================================================
find_package(Threads REQUIRED)
set(GFLAGS_USE_TARGET_NAMESPACE ON)
find_package(gflags CONFIG QUIET)
if(NOT gflags_FOUND)
    find_package(gflags QUIET)
endif()
if(NOT TARGET gflags)
    if(TARGET gflags::gflags_static)
        add_library(gflags INTERFACE IMPORTED)
        target_link_libraries(gflags INTERFACE gflags::gflags_static)
    elseif(TARGET gflags::gflags_nothreads_static)
        add_library(gflags INTERFACE IMPORTED)
        target_link_libraries(gflags INTERFACE gflags::gflags_nothreads_static)
    endif()
endif()
find_package(glog REQUIRED)
add_compile_definitions(GLOG_USE_GLOG_EXPORT)
if(TARGET glog::glog)
    if(TARGET gflags::gflags)
        target_link_libraries(glog::glog INTERFACE gflags::gflags)
    elseif(TARGET gflags)
        target_link_libraries(glog::glog INTERFACE gflags)
    endif()
    if(glog_DIR)
        if(EXISTS "${glog_DIR}/../../../include")
            target_include_directories(glog::glog BEFORE INTERFACE "${glog_DIR}/../../../include")
        elseif(EXISTS "${glog_DIR}/../../include")
            target_include_directories(glog::glog BEFORE INTERFACE "${glog_DIR}/../../include")
        endif()
    endif()
    if(gflags_DIR)
        if(EXISTS "${gflags_DIR}/../../../include")
            target_include_directories(glog::glog BEFORE INTERFACE "${gflags_DIR}/../../../include")
        elseif(EXISTS "${gflags_DIR}/../../include")
            target_include_directories(glog::glog BEFORE INTERFACE "${gflags_DIR}/../../include")
        endif()
    endif()
endif()

if(PELCOD_ENABLE_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)

    if(WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt
            HINTS
                "${Qt6_DIR}/../../../bin"
                "${CMAKE_PREFIX_PATH}/bin"
                "$ENV{QTDIR}/bin"
        )
    endif()
else()
    message(STATUS "PelcoD: Qt 6 integration disabled (PELCOD_ENABLE_QT=OFF)")
endif()

# ==============================================================================
# ONVIF Subsystem (PelcoDOnvif) Dependencies
# ==============================================================================
option(PELCOD_ENABLE_ONVIF "Build in-tree ONVIF Profile S client subsystem" ON)
if(PELCOD_ENABLE_ONVIF)
    find_package(CURL REQUIRED)
    find_package(pugixml CONFIG REQUIRED)
    find_package(OpenSSL REQUIRED)
    add_compile_definitions(PELCOD_ENABLE_ONVIF=1)
    message(STATUS "PelcoDOnvif: ONVIF Profile S subsystem enabled (curl, pugixml, OpenSSL)")
endif()

# ==============================================================================
# DJI Telemetry (DjiTelemetry) - MP4 tx3g subtitle telemetry -> MISB ST 0601
# ==============================================================================
option(PELCOD_ENABLE_DJI "Build DJI MP4 subtitle telemetry reader (depends on Klv only)" ON)

# ==============================================================================
# Video Subsystem (PelcoDVideo) Dependencies
# ==============================================================================
option(PELCOD_ENABLE_VIDEO "Build in-tree RTSP video decoder and overlay subsystem" ON)
option(PELCOD_ENABLE_FFMPEG "Enable FFmpeg decoder backend in PelcoDVideo" ON)
option(PELCOD_ENABLE_GSTREAMER "Enable GStreamer decoder backend in PelcoDVideo" ON)
option(PELCOD_ENABLE_LIVE555 "Enable Live555 + OpenH264 decoder backend in PelcoDVideo" ON)
option(PELCOD_ENABLE_FILTERS "Enable OpenCV video enhancement filters in PelcoDVideo" ON)

if(PELCOD_ENABLE_VIDEO AND PELCOD_ENABLE_QT)
    find_package(Qt6 COMPONENTS Qml Quick QuickControls2 QUIET)
endif()

if(PELCOD_ENABLE_VIDEO)
    # FFmpeg components
    if(PELCOD_ENABLE_FFMPEG)
        find_package(FFmpeg COMPONENTS avcodec avformat avutil swscale QUIET)
        if(NOT FFmpeg_FOUND AND NOT FFMPEG_FOUND)
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(FFMPEG QUIET
                    libavcodec
                    libavformat
                    libavutil
                    libswscale
                )
                if(FFMPEG_FOUND)
                    set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
                    set(FFMPEG_LIBRARIES ${FFMPEG_LINK_LIBRARIES})
                endif()
            endif()
        endif()
        if(NOT FFmpeg_FOUND AND NOT FFMPEG_FOUND)
            find_path(FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h PATHS "C:/vcpkg/installed/x64-windows/include" "$ENV{FFMPEG_ROOT}/include")
            find_library(AVCODEC_LIBRARY NAMES avcodec PATHS "C:/vcpkg/installed/x64-windows/lib" "$ENV{FFMPEG_ROOT}/lib")
            find_library(AVFORMAT_LIBRARY NAMES avformat PATHS "C:/vcpkg/installed/x64-windows/lib" "$ENV{FFMPEG_ROOT}/lib")
            find_library(AVUTIL_LIBRARY NAMES avutil PATHS "C:/vcpkg/installed/x64-windows/lib" "$ENV{FFMPEG_ROOT}/lib")
            find_library(SWSCALE_LIBRARY NAMES swscale PATHS "C:/vcpkg/installed/x64-windows/lib" "$ENV{FFMPEG_ROOT}/lib")
            if(FFMPEG_INCLUDE_DIR AND AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY AND SWSCALE_LIBRARY)
                set(FFMPEG_FOUND TRUE)
                set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIR})
                set(FFMPEG_LIBRARIES ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${SWSCALE_LIBRARY})
            endif()
        endif()
        if(FFmpeg_FOUND OR FFMPEG_FOUND)
            set(FFMPEG_FOUND TRUE)
            message(STATUS "PelcoDVideo: FFmpeg backend enabled")
        else()
            message(WARNING "PelcoDVideo: FFmpeg libraries not found, disabling FFmpeg backend")
            set(PELCOD_ENABLE_FFMPEG OFF)
        endif()
    endif()

    # GStreamer components
    if(PELCOD_ENABLE_GSTREAMER)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(GST QUIET
                gstreamer-1.0
                gstreamer-base-1.0
                gstreamer-app-1.0
                gstreamer-video-1.0
                glib-2.0
                gobject-2.0
            )
            if(GST_FOUND)
                set(GSTREAMER_FOUND TRUE)
                set(GSTREAMER_INCLUDE_DIRS ${GST_INCLUDE_DIRS})
                set(GSTREAMER_LIBRARIES ${GST_LINK_LIBRARIES})
            endif()
        endif()

        if(NOT GSTREAMER_FOUND)
            find_path(GST_INCLUDE_DIR NAMES gst/gst.h PATHS "C:/vcpkg/installed/x64-windows/include/gstreamer-1.0" "${GSTREAMER_ROOT}/include/gstreamer-1.0" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/gstreamer-1.0" PATH_SUFFIXES "gstreamer-1.0")
            find_path(GLIB_INCLUDE_DIR NAMES glib.h PATHS "C:/vcpkg/installed/x64-windows/include/glib-2.0" "${GSTREAMER_ROOT}/include/glib-2.0" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/glib-2.0" PATH_SUFFIXES "glib-2.0")
            find_path(GLIBCONFIG_INCLUDE_DIR NAMES glibconfig.h
                PATHS
                    "C:/vcpkg/installed/x64-windows/lib/glib-2.0/include"
                    "${GSTREAMER_ROOT}/lib/glib-2.0/include"
                    "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/glib-2.0/include"
                    "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/glib-2.0/include"
                    "/usr/lib64/glib-2.0/include"
                    "/usr/lib/glib-2.0/include"
                PATH_SUFFIXES "glib-2.0/include")

            find_library(GST_LIBRARY NAMES gstreamer-1.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
            find_library(GST_BASE_LIBRARY NAMES gstbase-1.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
            find_library(GST_APP_LIBRARY NAMES gstapp-1.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
            find_library(GST_VIDEO_LIBRARY NAMES gstvideo-1.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
            find_library(GLIB_LIBRARY NAMES glib-2.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
            find_library(GOBJECT_LIBRARY NAMES gobject-2.0 PATHS "C:/vcpkg/installed/x64-windows/lib" "${GSTREAMER_ROOT}/lib" "$ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")

            if(GST_INCLUDE_DIR AND GLIB_INCLUDE_DIR AND GLIBCONFIG_INCLUDE_DIR AND GST_LIBRARY AND GST_BASE_LIBRARY AND GST_APP_LIBRARY AND GST_VIDEO_LIBRARY AND GLIB_LIBRARY AND GOBJECT_LIBRARY)
                set(GSTREAMER_FOUND TRUE)
                set(GSTREAMER_INCLUDE_DIRS ${GST_INCLUDE_DIR} ${GLIB_INCLUDE_DIR} ${GLIBCONFIG_INCLUDE_DIR})
                set(GSTREAMER_LIBRARIES ${GST_LIBRARY} ${GST_BASE_LIBRARY} ${GST_APP_LIBRARY} ${GST_VIDEO_LIBRARY} ${GLIB_LIBRARY} ${GOBJECT_LIBRARY})
            endif()
        endif()

        if(GSTREAMER_FOUND)
            message(STATUS "PelcoDVideo: GStreamer backend enabled")
        else()
            message(WARNING "PelcoDVideo: GStreamer libraries not found, disabling GStreamer backend")
            set(PELCOD_ENABLE_GSTREAMER OFF)
        endif()
    endif()

    # Live555 and OpenH264 components
    if(PELCOD_ENABLE_LIVE555)
        include(FindOrFetchLive555)

        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(OPENH264 QUIET openh264)
            if(OPENH264_FOUND)
                set(OPENH264_INCLUDE_DIRS ${OPENH264_INCLUDE_DIRS})
                set(OPENH264_LIBRARIES ${OPENH264_LINK_LIBRARIES})
            endif()
        endif()

        if(NOT OPENH264_FOUND)
            find_path(OPENH264_INCLUDE_DIR NAMES wels/codec_api.h PATHS "C:/vcpkg/installed/x64-windows/include")
            find_library(OPENH264_LIB NAMES openh264 PATHS "C:/vcpkg/installed/x64-windows/lib")
            if(OPENH264_INCLUDE_DIR AND OPENH264_LIB)
                set(OPENH264_FOUND TRUE)
                set(OPENH264_INCLUDE_DIRS ${OPENH264_INCLUDE_DIR})
                set(OPENH264_LIBRARIES ${OPENH264_LIB})
            endif()
        endif()

        if(OPENH264_FOUND)
            message(STATUS "PelcoDVideo: OpenH264 library found")
        else()
            message(STATUS "PelcoDVideo: OpenH264 library not found")
        endif()

        if(LIVE555_FOUND AND OPENH264_FOUND)
            message(STATUS "PelcoDVideo: Live555 + OpenH264 backend enabled")
        else()
            if(NOT LIVE555_FOUND AND NOT OPENH264_FOUND)
                message(STATUS "PelcoDVideo: Neither Live555 nor OpenH264 was found, disabling Live555 backend")
            elseif(NOT LIVE555_FOUND)
                message(STATUS "PelcoDVideo: Live555 library not found (OpenH264 is available), disabling Live555 backend")
            else()
                message(STATUS "PelcoDVideo: OpenH264 library not found (Live555 is available), disabling Live555 backend")
            endif()
            set(PELCOD_ENABLE_LIVE555 OFF)
        endif()
    endif()

    # OpenCV video enhancement filters
    if(PELCOD_ENABLE_FILTERS)
        find_package(OpenCV COMPONENTS core imgproc video calib3d PATHS "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/share/opencv4" QUIET)
        if(OpenCV_FOUND AND NOT OpenCV_LIBS)
            set(OpenCV_LIBS ${OpenCV_LIBRARIES})
        endif()
        if(NOT OpenCV_FOUND)
            find_package(PkgConfig QUIET)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(OPENCV QUIET opencv4)
                if(NOT OPENCV_FOUND)
                    pkg_check_modules(OPENCV QUIET opencv)
                endif()
                if(OPENCV_FOUND)
                    set(OpenCV_FOUND TRUE)
                    set(OpenCV_VERSION ${OPENCV_VERSION})
                    set(OpenCV_INCLUDE_DIRS ${OPENCV_INCLUDE_DIRS})
                    set(OpenCV_LIBS ${OPENCV_LINK_LIBRARIES})
                endif()
            endif()
        endif()

        if(OpenCV_FOUND)
            message(STATUS "PelcoDVideo: OpenCV video enhancement filters enabled (version ${OpenCV_VERSION})")
        else()
            message(WARNING "PelcoDVideo: OpenCV libraries not found, disabling video enhancement filters")
            set(PELCOD_ENABLE_FILTERS OFF)
        endif()
    endif()
endif()

# ==============================================================================
# Installation Rules (GNUInstallDirs) & RPATH Settings
# ==============================================================================
include(GNUInstallDirs)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR};$ORIGIN/../lib")

# ==============================================================================
# Testing & Code Coverage Configuration
# ==============================================================================
option(BUILD_TESTING "Build unit tests" ON)
option(PELCOD_BUILD_TESTS "Build unit tests (alias for BUILD_TESTING)" ${BUILD_TESTING})
if(DEFINED PELCOD_BUILD_TESTS)
    set(BUILD_TESTING ${PELCOD_BUILD_TESTS})
endif()
option(PELCOD_ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang)" OFF)

if(BUILD_TESTING)
    include(Testing)
endif()


# ==============================================================================
# Subdirectories
# ==============================================================================
add_subdirectory(libs)

if(PELCOD_BUILD_APPS)
    if(PELCOD_ENABLE_QT AND TARGET Qt6::Widgets)
        add_subdirectory(app-pelcod-qt)
        add_subdirectory(app-fujinon-qt)
        add_subdirectory(app-visca-qt)
    endif()
    add_subdirectory(app-pelcod-tui)
    if(PELCOD_ENABLE_ONVIF AND PELCOD_ENABLE_QT AND TARGET Qt6::Widgets)
        add_subdirectory(app-onvif-qt)
        message(STATUS "Onvif: OnvifAppQt (app-onvif-qt) enabled")
    endif()
    if(PELCOD_ENABLE_VIDEO AND PELCOD_ENABLE_QT)
        if(TARGET Qt6::Quick AND TARGET Qt6::Qml AND TARGET Qt6::QuickControls2)
            add_subdirectory(app-video-qt)
            message(STATUS "PelcoD: VideoAppQt (app-video-qt) enabled")
            if(PELCOD_ENABLE_LIVE555 AND LIVE555_FOUND)
                add_subdirectory(app-rtsp-server-qt)
                message(STATUS "PelcoD: RtspServerAppQt (app-rtsp-server-qt) enabled")
            else()
                message(STATUS "PelcoD: RtspServerAppQt (app-rtsp-server-qt) disabled (requires Live555 backend)")
            endif()
        else()
            message(STATUS "PelcoD: VideoAppQt (app-video-qt) disabled (requires Qt6 Quick, Qml, and QuickControls2)")
            message(STATUS "PelcoD: RtspServerAppQt (app-rtsp-server-qt) disabled (requires Qt6 Quick, Qml, and QuickControls2)")
        endif()
    endif()

    if(PELCOD_ENABLE_QT AND TARGET Qt6::Quick AND TARGET Qt6::Qml)
        add_subdirectory(app-sightline-qt)
        message(STATUS "Sightline: SightlineAppQt (app-sightline-qt) enabled")
    else()
        message(STATUS "Sightline: SightlineAppQt (app-sightline-qt) disabled (requires Qt6 Quick and Qml)")
    endif()
endif()

if(PELCOD_ENABLE_ONVIF)
    add_subdirectory(tools/onvif_conformance)
endif()

add_subdirectory(tests)

# ==============================================================================
# Git Hooks & Clang-Format Integration
# ==============================================================================
include(GitHooks)

# ==============================================================================
# Target Installation
# ==============================================================================

set(PELCOD_CANDIDATE_TARGETS
    PelcoDAppQt
    FujinonAppQt
    ViscaAppQt
    PelcoDAppTui
    VideoAppQt
    RtspServerAppQt
    SightlineAppQt
    PelcoDCore
    PelcoDQt
    Math
    Transport
    ViscaCore
    ViscaSony
    Tracking
    Klv
    Mapping
    MappingQt
    PayloadHal
    PelcoDFujinon
    PelcoDSim
    Nmea
    SightlineCore
    SightlineQt
    Video
    VideoFilters
    DjiTelemetry
    Onvif
    OnvifAppQt
    OnvifServerApp
)

set(PELCOD_INSTALL_TARGETS)
foreach(_tgt IN LISTS PELCOD_CANDIDATE_TARGETS)
    if(TARGET ${_tgt})
        list(APPEND PELCOD_INSTALL_TARGETS ${_tgt})
    endif()
endforeach()

if(PELCOD_INSTALL_TARGETS)
    install(TARGETS ${PELCOD_INSTALL_TARGETS}
        EXPORT PelcoDTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(FILES
    libs/Math/Dct.h
    libs/Math/DctSharpnessEvaluator.h
    libs/Math/Dwt.h
    libs/Math/Fft.h
    libs/Math/GoertzelFilter.h
    libs/Math/IntegralImage.h
    libs/Math/MatrixMath.h
    libs/Math/NotchFilter.h
    libs/Math/PhaseCorrelation.h
    libs/Math/SpectrogramColorMap.h
    libs/Math/Stft.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Math
)

install(FILES
    libs/Transport/ITransport.h
    libs/Transport/TransportTypes.h
    libs/Transport/BaseTransport.h
    libs/Transport/SerialTransport.h
    libs/Transport/TcpTransport.h
    libs/Transport/UdpTransport.h
    libs/Transport/SocketUtils.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Transport
)

install(FILES
    libs/Tracking/ChirpCalibrator.h
    libs/Tracking/ExtendedKalmanFilter.h
    libs/Tracking/LatencyCalibrator.h
    libs/Tracking/LatencyEstimator.h
    libs/Tracking/OscillationDetector.h
    libs/Tracking/PidController.h
    libs/Tracking/PlantIdentifier.h
    libs/Tracking/PtzAutoTracker.h
    libs/Tracking/PtzCameraModel.h
    libs/Tracking/PtzSphericalEstimator.h
    libs/Tracking/Tracking.h
    libs/Tracking/TransientShockDetector.h
    libs/Tracking/UnscentedKalmanFilter.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Tracking
)

install(FILES
    libs/Klv/KlvTypes.h
    libs/Klv/KlvBer.h
    libs/Klv/KlvCrc.h
    libs/Klv/KlvEncoder.h
    libs/Klv/KlvParser.h
    libs/Klv/KlvStreamScanner.h
    libs/Klv/MpegTsKlvExtractor.h
    libs/Klv/KlvGeodesy.h
    libs/Klv/Misb1201.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Klv
)

if(PELCOD_ENABLE_ONVIF)
    install(FILES
        libs/Onvif/GeodesyUtils.h
        libs/Onvif/Onvif.h
        libs/Onvif/OnvifClient.h
        libs/Onvif/OnvifDiscovery.h
        libs/Onvif/OnvifHttpClient.h
        libs/Onvif/OnvifSecurity.h
        libs/Onvif/OnvifServer.h
        libs/Onvif/OnvifServerTypes.h
        libs/Onvif/OnvifTypes.h
        libs/Onvif/WsDiscoveryCommon.h
        libs/Onvif/WsDiscoveryServer.h
        libs/Onvif/XmlUtils.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Onvif
    )
    install(FILES
        libs/Onvif/adapters/PelcoDPtzAdapter.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Onvif/adapters
    )
endif()

install(FILES
    libs/PayloadHal/PayloadTypes.h
    libs/PayloadHal/IDevice.h
    libs/PayloadHal/IPanTiltUnit.h
    libs/PayloadHal/ILaserRangeFinder.h
    libs/PayloadHal/ICameraPayload.h
    libs/PayloadHal/IPayload.h
    libs/PayloadHal/GeoreferenceUtils.h
    libs/PayloadHal/PayloadFactory.h
    libs/PayloadHal/PayloadHal.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PayloadHal
)

install(FILES
    libs/PayloadHal/adapters/PelcoDPtzAdapter.h
    libs/PayloadHal/adapters/PelcoDFujinonPayloadAdapter.h
    libs/PayloadHal/adapters/ViscaSonyAdapter.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PayloadHal/adapters
)

install(FILES
    libs/PayloadHal/sim/SimulatedPayload.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PayloadHal/sim
)

install(FILES
    libs/PelcoDFujinon/FujinonBuilder.h
    libs/PelcoDFujinon/FujinonParser.h
    libs/PelcoDFujinon/FujinonSX800Device.h
    libs/PelcoDFujinon/FujinonTypes.h
    libs/PelcoDFujinon/PelcoDFujinon.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDFujinon
)

install(FILES
    libs/PelcoDSim/KinematicsSimulator.h
    libs/PelcoDSim/LatencyPipeline.h
    libs/PelcoDSim/MockPelcoDDevice.h
    libs/PelcoDSim/PelcoDSim.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDSim
)

install(FILES
    libs/PelcoDCore/DeviceStatus.h
    libs/PelcoDCore/ITransport.h
    libs/PelcoDCore/PacedCommandQueue.h
    libs/PelcoDCore/PelcoDDevice.h
    libs/PelcoDCore/PelcoDFrame.h
    libs/PelcoDCore/PelcoDTypes.h
    libs/PelcoDCore/ProtocolBuilder.h
    libs/PelcoDCore/ProtocolParser.h
    libs/PelcoDCore/RxStreamAccumulator.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDCore
)

if(TARGET PelcoDQt)
    install(FILES
        libs/PelcoDQt/QPelcoDDevice.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDQt
    )
endif()

if(PELCOD_ENABLE_VIDEO)
    install(FILES
        libs/Video/AtomicTripleBuffer.h
        libs/Video/BaseVideoDecoder.h
        libs/Video/BrailleRenderer.h
        libs/Video/DecoderFactory.h
        libs/Video/DecoderTypes.h
        libs/Video/DeviceEnumerator.h
        libs/Video/IVideoDecoder.h
        libs/Video/MockVideoDecoder.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Video
    )
    if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
        install(FILES
            libs/VideoFilters/ColorFilters.h
            libs/VideoFilters/SpatialFilters.h
            libs/VideoFilters/GeometricFilters.h
            libs/VideoFilters/ThermalFilters.h
            libs/VideoFilters/TrackingFilters.h
            libs/VideoFilters/OverlayFilters.h
            libs/VideoFilters/StreamHealthOsdFilter.h
            libs/VideoFilters/TacticalHudFilter.h
            libs/VideoFilters/VideoFilters.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/VideoFilters
        )
    endif()
endif()

install(EXPORT PelcoDTargets
    FILE PelcoDTargets.cmake
    NAMESPACE PelcoD::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PelcoD
)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE AND TARGET PelcoDAppQt)
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --no-compiler-runtime
                --no-translations
                \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/PelcoDAppQt.exe\"
        )
    ")
endif()

# ==============================================================================
# CPack Packaging Configuration
# ==============================================================================
set(CPACK_PACKAGE_NAME "PelcoDController")
set(CPACK_PACKAGE_VENDOR "Stavros Vagionitis")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-Platform Pelco-D PTZ Camera Controller & Protocol Inspector")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "Stavros Vagionitis <stavros@vagionitis.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Debian packaging options
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# RPM packaging options
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

# Package generators
if(NOT DEFINED CPACK_GENERATOR)
    if(WIN32)
        set(CPACK_GENERATOR "ZIP")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(CPACK_GENERATOR "TGZ")
    elseif(EXISTS "/etc/fedora-release" OR EXISTS "/etc/redhat-release")
        set(CPACK_GENERATOR "TGZ;RPM")
    elseif(EXISTS "/etc/debian_version")
        set(CPACK_GENERATOR "TGZ;DEB")
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()
endif()
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)

if(MSVC)
    message(STATUS "If using the 'Visual Studio' generator, now run")
    message(STATUS "     cmake --build . [target] --config <Config>")
    message(STATUS "If using the 'Ninja' generator, now run")
    message(STATUS "     cmake --build . [target]")
else()
    message(STATUS "If using the 'Unix Makefiles' generator, now run")
    message(STATUS "     cmake --build . [target] -- -j$(nproc)")
endif()
