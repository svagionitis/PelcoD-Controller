# ==============================================================================
# Live555 Streaming Media Library Discovery and Official Tarball Fallback
# ==============================================================================

if(POLICY CMP0100)
    cmake_policy(SET CMP0100 NEW)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIVE555 QUIET live555)
    if(LIVE555_FOUND)
        set(LIVE555_INCLUDE_DIRS ${LIVE555_INCLUDE_DIRS})
        set(LIVE555_LIBRARIES ${LIVE555_LINK_LIBRARIES})
    endif()
endif()

if(NOT LIVE555_FOUND)
    find_path(LIVE555_INCLUDE_DIR NAMES liveMedia.hh PATHS "C:/vcpkg/installed/x64-windows/include/liveMedia" PATH_SUFFIXES "liveMedia")
    find_library(LIVEMEDIA_LIB NAMES liveMedia PATHS "C:/vcpkg/installed/x64-windows/lib")
    find_library(GROUPSOCK_LIB NAMES groupsock PATHS "C:/vcpkg/installed/x64-windows/lib")
    find_library(BASICUSAGEENV_LIB NAMES BasicUsageEnvironment PATHS "C:/vcpkg/installed/x64-windows/lib")
    find_library(USAGEENV_LIB NAMES UsageEnvironment PATHS "C:/vcpkg/installed/x64-windows/lib")
    if(LIVE555_INCLUDE_DIR AND LIVEMEDIA_LIB AND GROUPSOCK_LIB AND BASICUSAGEENV_LIB AND USAGEENV_LIB)
        set(LIVE555_FOUND TRUE)
        set(LIVE555_INCLUDE_DIRS ${LIVE555_INCLUDE_DIR})
        set(LIVE555_LIBRARIES ${LIVEMEDIA_LIB} ${GROUPSOCK_LIB} ${BASICUSAGEENV_LIB} ${USAGEENV_LIB})
    endif()
endif()

# Fallback: Download official Live555 tarball from https://download.live555.com/live555-latest.tar.gz
if(NOT LIVE555_FOUND)
    set(LIVE555_TARBALL_URL "https://download.live555.com/live555-latest.tar.gz")
    set(LIVE555_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/_deps/live555")
    set(LIVE555_ARCHIVE_FILE "${LIVE555_DOWNLOAD_DIR}/live555-latest.tar.gz")
    set(LIVE555_SOURCE_DIR "${LIVE555_DOWNLOAD_DIR}/src")

    # 1. Search for existing extracted Live555 sources across candidate locations
    set(LIVE555_CANDIDATE_ROOTS
        "${LIVE555_SOURCE_DIR}/live"
        "${LIVE555_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/live"
        "${CMAKE_SOURCE_DIR}/build/live"
        "${CMAKE_SOURCE_DIR}/.cache/live555/src/live"
        "${CMAKE_SOURCE_DIR}/.cache/live555/src"
        "${CMAKE_SOURCE_DIR}/.cache/live555/live"
        "${CMAKE_SOURCE_DIR}/build/_deps/live555/src/live"
        "${CMAKE_SOURCE_DIR}/build/_deps/live555/src"
        "${CMAKE_SOURCE_DIR}/build_clang/_deps/live555/src/live"
        "${CMAKE_SOURCE_DIR}/build_clang/_deps/live555/src"
    )
    foreach(_cand ${LIVE555_CANDIDATE_ROOTS})
        if(EXISTS "${_cand}/liveMedia" AND EXISTS "${_cand}/BasicUsageEnvironment")
            set(LIVE555_ROOT_DIR "${_cand}")
            break()
        endif()
    endforeach()

    # 2. If not already extracted, check for cached tarball before attempting internet download
    if(NOT LIVE555_ROOT_DIR)
        set(LIVE555_CANDIDATE_TARBALLS
            "${CMAKE_SOURCE_DIR}/.cache/live555/live555-latest.tar.gz"
            "${CMAKE_SOURCE_DIR}/build_clang/_deps/live555/live555-latest.tar.gz"
            "${CMAKE_SOURCE_DIR}/build/_deps/live555/live555-latest.tar.gz"
            "${LIVE555_ARCHIVE_FILE}"
        )
        set(VALID_TARBALL "")
        foreach(_tarball ${LIVE555_CANDIDATE_TARBALLS})
            if(EXISTS "${_tarball}")
                file(SIZE "${_tarball}" _t_size)
                if(_t_size GREATER 1000)
                    set(VALID_TARBALL "${_tarball}")
                    break()
                endif()
            endif()
        endforeach()

        if(VALID_TARBALL)
            message(STATUS "PelcoDVideo: Using cached Live555 tarball: ${VALID_TARBALL}")
            file(MAKE_DIRECTORY "${LIVE555_SOURCE_DIR}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${VALID_TARBALL}"
                WORKING_DIRECTORY "${LIVE555_SOURCE_DIR}"
                RESULT_VARIABLE LIVE555_EXTRACT_RESULT
            )
            if(EXISTS "${LIVE555_SOURCE_DIR}/live/liveMedia")
                set(LIVE555_ROOT_DIR "${LIVE555_SOURCE_DIR}/live")
            elseif(EXISTS "${LIVE555_SOURCE_DIR}/liveMedia")
                set(LIVE555_ROOT_DIR "${LIVE555_SOURCE_DIR}")
            endif()
        endif()
    endif()

    # 3. If still not found, attempt download
    if(NOT LIVE555_ROOT_DIR)
        message(STATUS "PelcoDVideo: Live555 not found on system. Attempting download from ${LIVE555_TARBALL_URL}...")
        file(MAKE_DIRECTORY "${LIVE555_DOWNLOAD_DIR}")
        
        file(DOWNLOAD
            "${LIVE555_TARBALL_URL}"
            "${LIVE555_ARCHIVE_FILE}"
            STATUS LIVE555_DL_STATUS
            LOG LIVE555_DL_LOG
            TIMEOUT 30
        )

        list(GET LIVE555_DL_STATUS 0 LIVE555_DL_CODE)
        list(GET LIVE555_DL_STATUS 1 LIVE555_DL_MSG)

        if(LIVE555_DL_CODE EQUAL 0)
            file(SIZE "${LIVE555_ARCHIVE_FILE}" _dl_size)
            if(_dl_size GREATER 1000)
                message(STATUS "PelcoDVideo: Live555 tarball downloaded successfully. Extracting...")
                file(MAKE_DIRECTORY "${LIVE555_SOURCE_DIR}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E tar xzf "${LIVE555_ARCHIVE_FILE}"
                    WORKING_DIRECTORY "${LIVE555_SOURCE_DIR}"
                    RESULT_VARIABLE LIVE555_EXTRACT_RESULT
                )
                if(EXISTS "${LIVE555_SOURCE_DIR}/live/liveMedia")
                    set(LIVE555_ROOT_DIR "${LIVE555_SOURCE_DIR}/live")
                elseif(EXISTS "${LIVE555_SOURCE_DIR}/liveMedia")
                    set(LIVE555_ROOT_DIR "${LIVE555_SOURCE_DIR}")
                endif()
            else()
                file(REMOVE "${LIVE555_ARCHIVE_FILE}")
                message(WARNING "PelcoDVideo: Downloaded Live555 tarball is corrupted or empty.")
            endif()
        else()
            if(EXISTS "${LIVE555_ARCHIVE_FILE}")
                file(REMOVE "${LIVE555_ARCHIVE_FILE}")
            endif()
            message(WARNING "PelcoDVideo: Could not download Live555 (${LIVE555_DL_MSG}). Offline or download unavailable.")
        endif()
    endif()

    if(LIVE555_ROOT_DIR)
        set(LIVE555_INCLUDE_DIRS
            "${LIVE555_ROOT_DIR}/liveMedia/include"
            "${LIVE555_ROOT_DIR}/groupsock/include"
            "${LIVE555_ROOT_DIR}/UsageEnvironment/include"
            "${LIVE555_ROOT_DIR}/BasicUsageEnvironment/include"
        )

        file(GLOB LIVEMEDIA_SRCS "${LIVE555_ROOT_DIR}/liveMedia/*.cpp" "${LIVE555_ROOT_DIR}/liveMedia/*.c")
        file(GLOB GROUPSOCK_SRCS "${LIVE555_ROOT_DIR}/groupsock/*.cpp" "${LIVE555_ROOT_DIR}/groupsock/*.c")
        file(GLOB USAGEENV_SRCS "${LIVE555_ROOT_DIR}/UsageEnvironment/*.cpp")
        file(GLOB BASICUSAGEENV_SRCS "${LIVE555_ROOT_DIR}/BasicUsageEnvironment/*.cpp")

        enable_language(C)

        add_library(liveMedia_in_tree STATIC ${LIVEMEDIA_SRCS})
        add_library(groupsock_in_tree STATIC ${GROUPSOCK_SRCS})
        add_library(UsageEnvironment_in_tree STATIC ${USAGEENV_SRCS})
        add_library(BasicUsageEnvironment_in_tree STATIC ${BASICUSAGEENV_SRCS})

        foreach(_target liveMedia_in_tree groupsock_in_tree UsageEnvironment_in_tree BasicUsageEnvironment_in_tree)
            foreach(_dir ${LIVE555_INCLUDE_DIRS})
                target_include_directories(${_target} PUBLIC $<BUILD_INTERFACE:${_dir}>)
            endforeach()
            target_compile_definitions(${_target} PUBLIC NO_OPENSSL=1 NO_STD_LIB=1 NO_SSTREAM=1 SOCKLEN_T=socklen_t)
            if(NOT WIN32)
                target_compile_definitions(${_target} PUBLIC _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE)
            endif()
            if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
                target_compile_options(${_target} PRIVATE -include limits.h)
            endif()
            set_target_properties(${_target} PROPERTIES
                POSITION_INDEPENDENT_CODE ON
                AUTOMOC OFF
                AUTOUIC OFF
                AUTORCC OFF
            )
            if(WIN32)
                target_compile_definitions(${_target} PUBLIC _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS NO_GETIFADDRS=1)
            endif()
            if(MSVC)
                if(ENABLE_HARDENING)
                    target_compile_options(${_target} PRIVATE
                        /guard:cf
                        /guard:ehcont
                    )
                    if(ENABLE_SPECTRE_MITIGATION)
                        target_compile_options(${_target} PRIVATE /Qspectre)
                    endif()
                    target_compile_definitions(${_target} PRIVATE
                        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:_ITERATOR_DEBUG_LEVEL=1>
                    )
                endif()
            endif()
        endforeach()

        if(WIN32)
            target_link_libraries(liveMedia_in_tree PUBLIC ws2_32)
            target_link_libraries(groupsock_in_tree PUBLIC ws2_32)
            target_link_libraries(BasicUsageEnvironment_in_tree PUBLIC ws2_32)
        endif()

        install(TARGETS liveMedia_in_tree groupsock_in_tree UsageEnvironment_in_tree BasicUsageEnvironment_in_tree
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )

        set(LIVE555_LIBRARIES liveMedia_in_tree groupsock_in_tree BasicUsageEnvironment_in_tree UsageEnvironment_in_tree)
        set(LIVE555_FOUND TRUE)
        message(STATUS "PelcoDVideo: Live555 in-tree targets configured successfully from official tarball.")
    endif()
endif()
