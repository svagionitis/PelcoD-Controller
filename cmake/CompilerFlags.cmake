# Compiler flags, warnings, security hardening, and sanitizers
# Supports Linux (GCC/Clang) and Windows (MSVC)
# Modeled after cmake/CompilerFlags.cmake

option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ENABLE_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (TSan)" OFF)
option(ENABLE_HARDENING "Enable security hardening flags" ON)

function(apply_compiler_flags TARGET_NAME)
    set_target_properties(${TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if(WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _CRT_SECURE_NO_WARNINGS
        )
    endif()

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /W4 /wd4324 /permissive- /FS)
        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE /WX)
        endif()

        if(ENABLE_HARDENING)
            target_compile_options(${TARGET_NAME} PRIVATE /GS /guard:cf)
            target_link_options(${TARGET_NAME} PRIVATE /NXCOMPAT /DYNAMICBASE /HIGHENTROPYVA /guard:cf)
        endif()
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wconversion
            -Wsign-conversion
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE -Werror)
        endif()

        if(ENABLE_HARDENING AND NOT EMSCRIPTEN)
            get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
            target_compile_options(${TARGET_NAME} PRIVATE
                -fstack-protector-strong
                -fstack-clash-protection
            )

            if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
                target_compile_options(${TARGET_NAME} PRIVATE -fcf-protection=full)
            endif()

            if(TARGET_TYPE STREQUAL "EXECUTABLE")
                target_compile_options(${TARGET_NAME} PRIVATE -fPIE)
                target_link_options(${TARGET_NAME} PRIVATE -pie)
            endif()

            if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
                target_compile_definitions(${TARGET_NAME} PRIVATE _FORTIFY_SOURCE=2)
            endif()

            target_link_options(${TARGET_NAME} PRIVATE
                -Wl,-z,relro,-z,now
                -Wl,-z,noexecstack
            )
        endif()

        set(SANITIZER_FLAGS "")
        if(ENABLE_ASAN)
            list(APPEND SANITIZER_FLAGS "-fsanitize=address" "-fno-omit-frame-pointer")
        endif()

        if(ENABLE_UBSAN)
            list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
        endif()

        if(ENABLE_TSAN)
            if(ENABLE_ASAN)
                message(FATAL_ERROR "TSan cannot be combined with ASan.")
            endif()
            list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
        endif()

        if(SANITIZER_FLAGS)
            target_compile_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
            target_link_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAGS})
        endif()
    endif()
endfunction()
