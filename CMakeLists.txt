cmake_minimum_required(VERSION 3.16)
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

install(TARGETS PelcoDAppQt PelcoDAppTui PelcoDCore PelcoDQt
    EXPORT PelcoDTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES
    libs/PelcoDCore/CircularByteRing.h
    libs/PelcoDCore/DeviceStatus.h
    libs/PelcoDCore/ITransport.h
    libs/PelcoDCore/MockPelcoDDevice.h
    libs/PelcoDCore/PelcoDDevice.h
    libs/PelcoDCore/PelcoDFrame.h
    libs/PelcoDCore/PelcoDTypes.h
    libs/PelcoDCore/ProtocolBuilder.h
    libs/PelcoDCore/ProtocolParser.h
    libs/PelcoDCore/SerialTransport.h
    libs/PelcoDCore/TcpTransport.h
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
