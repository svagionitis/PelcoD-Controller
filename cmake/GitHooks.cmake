# Git pre-commit hook and formatting integration
# Manages automated installation of git pre-commit hooks and formatting targets.

find_package(Git QUIET)

# Determine the git hooks directory
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path hooks
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_HOOKS_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_HOOKS_PATH)
        if(NOT IS_ABSOLUTE "${GIT_HOOKS_PATH}")
            set(GIT_HOOKS_DIR "${PROJECT_SOURCE_DIR}/${GIT_HOOKS_PATH}")
        else()
            set(GIT_HOOKS_DIR "${GIT_HOOKS_PATH}")
        endif()
    endif()
endif()

if(NOT GIT_HOOKS_DIR)
    set(GIT_HOOKS_DIR "${PROJECT_SOURCE_DIR}/.git/hooks")
endif()

option(PELCOD_INSTALL_GIT_HOOKS "Automatically install git pre-commit hook during CMake configure" ON)

# Automated installation of the pre-commit hook during CMake configuration
if(PELCOD_INSTALL_GIT_HOOKS AND EXISTS "${PROJECT_SOURCE_DIR}/.git" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/pre-commit")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_LIST_DIR}/pre-commit"
            "${GIT_HOOKS_DIR}/pre-commit"
        RESULT_VARIABLE _GIT_HOOK_COPY_RES
        ERROR_QUIET
    )
    if(_GIT_HOOK_COPY_RES EQUAL 0)
        message(STATUS "Git pre-commit hook installed to ${GIT_HOOKS_DIR}/pre-commit")
    endif()
endif()

# Target to explicitly install/update git hooks
add_custom_target(install-git-hooks
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_LIST_DIR}/pre-commit"
        "${GIT_HOOKS_DIR}/pre-commit"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Installing git pre-commit hook to ${GIT_HOOKS_DIR}"
)

# Target to execute the pre-commit hook manually
add_custom_target(pre-commit
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/pre-commit"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Running git pre-commit hook"
)

# Code formatting targets using clang-format
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-15 clang-format-14 clang-format-12)
if(CLANG_FORMAT_BIN)
    file(GLOB_RECURSE ALL_CXX_SOURCES
        "${PROJECT_SOURCE_DIR}/libs/*.cpp"
        "${PROJECT_SOURCE_DIR}/libs/*.h"
        "${PROJECT_SOURCE_DIR}/libs/*.hpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-qt/*.cpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-qt/*.h"
        "${PROJECT_SOURCE_DIR}/app-pelcod-qt/*.hpp"
        "${PROJECT_SOURCE_DIR}/app-visca-qt/*.cpp"
        "${PROJECT_SOURCE_DIR}/app-visca-qt/*.h"
        "${PROJECT_SOURCE_DIR}/app-visca-qt/*.hpp"
        "${PROJECT_SOURCE_DIR}/app-video-qt/*.cpp"
        "${PROJECT_SOURCE_DIR}/app-video-qt/*.h"
        "${PROJECT_SOURCE_DIR}/app-video-qt/*.hpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-tui/*.cpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-tui/*.h"
        "${PROJECT_SOURCE_DIR}/app-pelcod-tui/*.hpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-tui/views/*.cpp"
        "${PROJECT_SOURCE_DIR}/app-pelcod-tui/views/*.h"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.h"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i -style=file ${ALL_CXX_SOURCES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Formatting all C++ sources with clang-format"
    )
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror -style=file ${ALL_CXX_SOURCES}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking C++ sources formatting with clang-format"
    )
    add_custom_target(format-check
        DEPENDS check-format
        COMMENT "Checking C++ sources formatting with clang-format (alias)"
    )
endif()

# IWYU and <cstdint> compliance target
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(check-iwyu
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/scripts/verify_iwyu_cstdint.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Verifying <cstdint> and IWYU compliance across C++ sources"
    )
    add_custom_target(check-type-conversions
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/scripts/verify_type_conversions.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Verifying MISRA 7.0.6 and CERT INT31-C range conversions across C++ sources"
    )
endif()
