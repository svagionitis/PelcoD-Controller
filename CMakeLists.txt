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
# Testing & Code Coverage Configuration
# ==============================================================================
option(BUILD_TESTING "Build unit tests" ON)
option(PELCOD_ENABLE_COVERAGE "Enable code coverage instrumentation (GCC/Clang)" OFF)

if(PELCOD_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "PelcoD: Code coverage enabled (--coverage -O0 -g)")
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    else()
        message(STATUS "PelcoD: PELCOD_ENABLE_COVERAGE active on MSVC. Use OpenCppCoverage for execution analysis.")
    endif()
endif()

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest CONFIG QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "PelcoD: GTest not found in system/vcpkg. Fetching GoogleTest via FetchContent...")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(_PELCOD_SAVED_AUTOUIC ${CMAKE_AUTOUIC})
        set(_PELCOD_SAVED_AUTOMOC ${CMAKE_AUTOMOC})
        set(_PELCOD_SAVED_AUTORCC ${CMAKE_AUTORCC})
        set(CMAKE_AUTOUIC OFF)
        set(CMAKE_AUTOMOC OFF)
        set(CMAKE_AUTORCC OFF)

        FetchContent_MakeAvailable(googletest)

        set(CMAKE_AUTOUIC ${_PELCOD_SAVED_AUTOUIC})
        set(CMAKE_AUTOMOC ${_PELCOD_SAVED_AUTOMOC})
        set(CMAKE_AUTORCC ${_PELCOD_SAVED_AUTORCC})

        if(TARGET gtest)
            set_target_properties(gtest PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        endif()
        if(TARGET gtest_main)
            set_target_properties(gtest_main PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        endif()
        if(TARGET gmock)
            set_target_properties(gmock PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        endif()
        if(TARGET gmock_main)
            set_target_properties(gmock_main PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
        endif()
    endif()
    include(GoogleTest)

    function(pelcod_register_gtest target_name)
        cmake_parse_arguments(ARG "NO_MAIN" "" "" ${ARGN})
        if(ARG_NO_MAIN)
            target_link_libraries(${target_name} PRIVATE GTest::gmock)
        else()
            target_link_libraries(${target_name} PRIVATE GTest::gmock_main)
        endif()
        apply_compiler_flags(${target_name})

        if(WIN32)
            if(TARGET Qt6::Core)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::Core>"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying Qt6 Core DLL for ${target_name}"
                )
            endif()
            if(TARGET Qt6::Gui)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::Gui>"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying Qt6 Gui DLL for ${target_name}"
                )
            endif()
            if(TARGET Qt6::Widgets)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:Qt6::Widgets>"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying Qt6 Widgets DLL for ${target_name}"
                )
            endif()
            if(TARGET glog::glog)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "$<TARGET_FILE:glog::glog>"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying glog DLL for ${target_name}"
                )
            endif()
            find_file(_GFLAGS_DEBUG_DLL NAMES gflags_debug.dll gflags.dll
                PATHS "C:/vcpkg/installed/x64-windows/debug/bin" "C:/vcpkg/installed/x64-windows/bin"
            )
            if(_GFLAGS_DEBUG_DLL)
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_GFLAGS_DEBUG_DLL}"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying gflags DLL for ${target_name}"
                )
            endif()
            gtest_discover_tests(${target_name}
                DISCOVERY_MODE PRE_TEST
                PROPERTIES ENVIRONMENT "PATH=C:/Qt/6.6.1/msvc2019_64/bin\;C:/vcpkg/installed/x64-windows/debug/bin\;C:/vcpkg/installed/x64-windows/bin\;$ENV{PATH};QT_PLUGIN_PATH=C:/Qt/6.6.1/msvc2019_64/plugins;QT_QPA_PLATFORM=offscreen"
            )
        else()
            gtest_discover_tests(${target_name}
                DISCOVERY_MODE PRE_TEST
            )
        endif()
    endfunction()

    if(WIN32)
        find_program(OPENCPPCOVERAGE_EXE NAMES OpenCppCoverage.exe PATHS "C:/Program Files/OpenCppCoverage")
        if(OPENCPPCOVERAGE_EXE)
            find_program(POWERSHELL_EXE NAMES pwsh powershell)
            if(NOT POWERSHELL_EXE)
                set(POWERSHELL_EXE powershell)
            endif()
            add_custom_target(coverage
                COMMAND "${POWERSHELL_EXE}" -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/scripts/RunCoverage.ps1"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                COMMENT "Generating unified code coverage report with OpenCppCoverage..."
            )
        endif()
    endif()
endif()


# ==============================================================================
# Subdirectories
# ==============================================================================
add_subdirectory(libs)
add_subdirectory(app-pelcod-qt)
add_subdirectory(app-visca-qt)
add_subdirectory(app-pelcod-tui)

add_subdirectory(tests)

# ==============================================================================
# Git Hooks & Clang-Format Integration
# ==============================================================================
include(GitHooks)

# ==============================================================================
# Target Installation
# ==============================================================================

set(PELCOD_INSTALL_TARGETS PelcoDAppQt ViscaAppQt PelcoDAppTui PelcoDCore PelcoDQt PelcoDMath Transport ViscaCore ViscaSony PelcoDTracking PelcoDFujinon PelcoDSim)
if(PELCOD_ENABLE_ONVIF)
    list(APPEND PELCOD_INSTALL_TARGETS PelcoDOnvif)
endif()
if(PELCOD_ENABLE_VIDEO)
    list(APPEND PELCOD_INSTALL_TARGETS Video)
    if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
        list(APPEND PELCOD_INSTALL_TARGETS VideoFilters)
    endif()
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
    libs/PelcoDTracking/ChirpCalibrator.h
    libs/PelcoDTracking/ExtendedKalmanFilter.h
    libs/PelcoDTracking/LatencyCalibrator.h
    libs/PelcoDTracking/LatencyEstimator.h
    libs/PelcoDTracking/OscillationDetector.h
    libs/PelcoDTracking/PelcoDTracking.h
    libs/PelcoDTracking/PidController.h
    libs/PelcoDTracking/PlantIdentifier.h
    libs/PelcoDTracking/PtzAutoTracker.h
    libs/PelcoDTracking/PtzCameraModel.h
    libs/PelcoDTracking/PtzSphericalEstimator.h
    libs/PelcoDTracking/TransientShockDetector.h
    libs/PelcoDTracking/UnscentedKalmanFilter.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDTracking
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

install(FILES
    libs/PelcoDQt/QPelcoDDevice.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/PelcoDQt
)

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
