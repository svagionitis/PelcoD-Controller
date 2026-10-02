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
        target_compile_options(${TARGET_NAME} PRIVATE
            /W4                     # Baseline high warning level (Level 4)
            /wd4324                 # Suppress C4324: structure padded due to alignment specifier (alignas/SIMD)
            /permissive-            # Enforce strict standard C++ conformance (disables Microsoft non-standard extensions)
            /FS                     # Force synchronous PDB access across concurrent compilation jobs
            /external:anglebrackets # Treat angle-bracket headers (#include <...>) as external library headers
            /external:W0            # Suppress warnings inside external headers (Qt, STL, external third-party libs)
            /w14244                 # C4244: conversion from 'type1' to 'type2', possible loss of data (-Wconversion)
            /w14267                 # C4267: conversion from 'size_t' to smaller integer type, possible loss of data
            /w14365                 # C4365: signed to unsigned conversion mismatch (-Wsign-conversion)
            /w14388                 # C4388: signed/unsigned mismatch in relational comparison operations
            /w14265                 # C4265: class has virtual functions but non-virtual destructor (-Wnon-virtual-dtor)
            /w14263                 # C4263: member function does not override any base class virtual member (-Woverloaded-virtual)
            /w14264                 # C4264: no override available for virtual member; base function is hidden
            /w14456                 # C4456: declaration hides previous local variable (-Wshadow)
            /w14457                 # C4457: declaration hides function parameter (-Wshadow)
            /w14458                 # C4458: declaration hides class member (-Wshadow)
            /w15038                 # C5038: member initialization order mismatch (-Wreorder)
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE /WX) # Treat all warnings as fatal errors (-Werror)
        endif()

        if(ENABLE_HARDENING)
            target_compile_options(${TARGET_NAME} PRIVATE
                /GS                 # Buffer security check (stack canary protection against buffer overflows)
                /guard:cf           # Control Flow Guard (compiler-level indirect call target validation)
                /sdl                # Microsoft Security Development Lifecycle checks (strict security checks)
            )
            target_link_options(${TARGET_NAME} PRIVATE
                /NXCOMPAT           # Data Execution Prevention (DEP / No-Execute stack, equivalent to -z noexecstack)
                /DYNAMICBASE        # Address Space Layout Randomization (ASLR, equivalent to -pie)
                /HIGHENTROPYVA      # 64-bit ASLR with 64-bit high entropy address space
                /guard:cf           # Linker Control Flow Guard (emits valid address table for indirect calls)
                /CETCOMPAT          # Hardware Control-flow Enforcement Technology (Hardware Shadow Stack on x64)
                /DEPENDENTLOADFLAG:0x800 # Restrict DLL loading to %SystemRoot%\System32 and application directory (anti-DLL hijacking)
            )
        endif()
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall                   # Enable core compiler warning diagnostics
            -Wextra                 # Enable supplementary warning checks
            -Wpedantic              # Enforce strict ISO C++ conformance
            -Wshadow                # Warn when a variable shadows another in outer scope
            -Wnon-virtual-dtor      # Warn when class has virtual functions but non-virtual destructor
            -Wold-style-cast        # Warn on C-style casts; prefer static_cast / reinterpret_cast
            -Wcast-align            # Warn when pointer cast increases alignment requirements
            -Wunused                # Warn on unused variables, functions, and parameters
            -Woverloaded-virtual    # Warn when function hides base class virtual function
            -Wnull-dereference      # Warn when compiler detects execution path that dereferences null
            -Wdouble-promotion      # Warn when float is implicitly promoted to double
            -Wformat=2              # Strict format string checks (printf/scanf)
            -Wconversion            # Warn on implicit conversions that may alter numerical values
            -Wsign-conversion       # Warn on implicit conversions between signed and unsigned types
        )

        if(WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET_NAME} PRIVATE -Werror) # Treat all compiler warnings as fatal errors
        endif()

        if(ENABLE_HARDENING AND NOT EMSCRIPTEN)
            get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
            target_compile_options(${TARGET_NAME} PRIVATE
                -fstack-protector-strong # Canary-based stack buffer overflow protection
                -fstack-clash-protection # Prevent stack clash attacks across page boundaries
            )

            if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
                target_compile_options(${TARGET_NAME} PRIVATE -fcf-protection=full) # Control Flow Integrity (IBT + SHSTK)
            endif()

            if(TARGET_TYPE STREQUAL "EXECUTABLE")
                target_compile_options(${TARGET_NAME} PRIVATE -fPIE) # Position Independent Executable code generation
                target_link_options(${TARGET_NAME} PRIVATE -pie)     # Produce Position Independent Executable
            endif()

            if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
                target_compile_definitions(${TARGET_NAME} PRIVATE _FORTIFY_SOURCE=2) # Runtime buffer boundary checks
            endif()

            target_link_options(${TARGET_NAME} PRIVATE
                -Wl,-z,relro,-z,now # Full Read-Only Relocation (RELRO) and immediate binding
                -Wl,-z,noexecstack  # Mark executable stack as non-executable (NX/DEP)
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
