cmake_minimum_required(VERSION 3.16)
project(PelcoDController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

# ==============================================================================
# Subdirectories
# ==============================================================================
add_subdirectory(libs)
add_subdirectory(app-qt)

enable_testing()
add_subdirectory(tests)

# ==============================================================================
# Clang-Format Targets & Git Pre-Commit Hook Installation
# ==============================================================================
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-15 clang-format-14 clang-format-12)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/app-qt/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/app-qt/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${ALL_SOURCE_FILES}
        COMMENT "Applying clang-format..."
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${ALL_SOURCE_FILES}
        COMMENT "Checking clang-format compliance..."
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    set(GIT_HOOK_SRC "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pre-commit")
    set(GIT_HOOK_DST "${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks/pre-commit")
    if(EXISTS "${GIT_HOOK_SRC}")
        configure_file("${GIT_HOOK_SRC}" "${GIT_HOOK_DST}" COPYONLY)
        file(CHMOD "${GIT_HOOK_DST}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        message(STATUS "Successfully installed Git pre-commit hook for formatting")
    endif()
endif()

