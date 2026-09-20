cmake_minimum_required(VERSION 3.16)
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
# Dependencies
# ==============================================================================
find_package(Threads REQUIRED)
set(GFLAGS_USE_TARGET_NAMESPACE ON)
find_package(gflags CONFIG REQUIRED)
find_package(glog REQUIRED)

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
# Video Subsystem (PelcoDVideo) Dependencies
# ==============================================================================
option(PELCOD_ENABLE_VIDEO "Build in-tree RTSP video decoder and overlay subsystem" ON)
option(PELCOD_ENABLE_FFMPEG "Enable FFmpeg decoder backend in PelcoDVideo" ON)
option(PELCOD_ENABLE_GSTREAMER "Enable GStreamer decoder backend in PelcoDVideo" ON)
option(PELCOD_ENABLE_FILTERS "Enable OpenCV video enhancement filters in PelcoDVideo" ON)

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

    # OpenCV video enhancement filters
    if(PELCOD_ENABLE_FILTERS)
        find_package(OpenCV COMPONENTS core imgproc video calib3d QUIET)
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
# Subdirectories
# ==============================================================================
add_subdirectory(libs)
add_subdirectory(app-qt)
add_subdirectory(app-tui)

enable_testing()
add_subdirectory(tests)

# ==============================================================================
# Git Hooks & Clang-Format Integration
# ==============================================================================
include(GitHooks)

# ==============================================================================
# Target Installation
# ==============================================================================

set(PELCOD_INSTALL_TARGETS PelcoDAppQt PelcoDAppTui PelcoDCore PelcoDQt PelcoDMath PelcoDTransport)
if(PELCOD_ENABLE_ONVIF)
    list(APPEND PELCOD_INSTALL_TARGETS PelcoDOnvif)
endif()
if(PELCOD_ENABLE_VIDEO)
    list(APPEND PELCOD_INSTALL_TARGETS PelcoDVideo)
endif()

install(TARGETS ${PELCOD_INSTALL_TARGETS}
    EXPORT PelcoDTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    libs/PelcoDMath/Dct.h
    libs/PelcoDMath/DctSharpnessEvaluator.h
    libs/PelcoDMath/Dwt.h
    libs/PelcoDMath/Fft.h
    libs/PelcoDMath/GoertzelFilter.h
    libs/PelcoDMath/IntegralImage.h
    libs/PelcoDMath/MatrixMath.h
    libs/PelcoDMath/NotchFilter.h
    libs/PelcoDMath/PhaseCorrelation.h
    libs/PelcoDMath/SpectrogramColorMap.h
    libs/PelcoDMath/Stft.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDMath
)

install(FILES
    libs/PelcoDTransport/BaseTransport.h
    libs/PelcoDTransport/SerialTransport.h
    libs/PelcoDTransport/TcpTransport.h
    libs/PelcoDTransport/UdpTransport.h
    libs/PelcoDTransport/SocketUtils.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDTransport
)

install(FILES
    libs/PelcoDCore/DeviceStatus.h
    libs/PelcoDCore/ITransport.h
    libs/PelcoDCore/MockPelcoDDevice.h
    libs/PelcoDCore/PelcoDDevice.h
    libs/PelcoDCore/PelcoDFrame.h
    libs/PelcoDCore/PelcoDTypes.h
    libs/PelcoDCore/ProtocolBuilder.h
    libs/PelcoDCore/ProtocolParser.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDCore
)

install(FILES
    libs/PelcoDQt/QPelcoDDevice.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDQt
)

install(EXPORT PelcoDTargets
    FILE PelcoDTargets.cmake
    NAMESPACE PelcoD::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/PelcoD
)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
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
set(CPACK_GENERATOR "TGZ;DEB")
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
