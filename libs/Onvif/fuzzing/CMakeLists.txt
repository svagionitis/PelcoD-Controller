# ==============================================================================
# libs/Onvif/fuzzing/CMakeLists.txt
# Continuous Fuzz Testing via LLVM libFuzzer & Deterministic Regression Runner
# ==============================================================================

option(PELCOD_ENABLE_FUZZING "Build LLVM libFuzzer harnesses for ONVIF" OFF)

set(FUZZ_HARNESSES
    fuzz_soap_router
    fuzz_ws_discovery
    fuzz_client_xml
    fuzz_auth_headers
    fuzz_url_validator
)

# ------------------------------------------------------------------------------
# Mode A: Active libFuzzer Build (requires Clang)
# ------------------------------------------------------------------------------
if(PELCOD_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "PELCOD_ENABLE_FUZZING requires Clang compiler (found: ${CMAKE_CXX_COMPILER_ID}).")
    endif()

    set(FUZZ_COMPILE_FLAGS
        -fsanitize=fuzzer,address,undefined
        -fno-omit-frame-pointer
        -g
        -O1
    )

    set(FUZZ_LINK_FLAGS
        -fsanitize=fuzzer,address,undefined
    )

    foreach(TARGET_NAME ${FUZZ_HARNESSES})
        add_executable(${TARGET_NAME} "harnesses/${TARGET_NAME}.cpp")
        target_include_directories(${TARGET_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_SOURCE_DIR}/../services
            ${CMAKE_CURRENT_SOURCE_DIR}/../adapters
            ${CMAKE_SOURCE_DIR}/libs
        )
        target_compile_options(${TARGET_NAME} PRIVATE ${FUZZ_COMPILE_FLAGS})
        target_link_options(${TARGET_NAME} PRIVATE ${FUZZ_LINK_FLAGS})
        target_link_libraries(${TARGET_NAME} PRIVATE Onvif)
        apply_compiler_flags(${TARGET_NAME})

        if(BUILD_TESTING)
            string(REPLACE "fuzz_" "" SUB_NAME "${TARGET_NAME}")
            add_test(
                NAME "FuzzSmoke_${TARGET_NAME}"
                COMMAND ${TARGET_NAME} -runs=1000 -max_len=65536
                        "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${SUB_NAME}"
            )
        endif()
    endforeach()
endif()

# ------------------------------------------------------------------------------
# Mode B: Deterministic Regression Suite (Compatible with GCC, MSVC, Clang)
# ------------------------------------------------------------------------------
if(BUILD_TESTING)
    add_executable(TestFuzzRegression
        standalone/TestFuzzRegression.cpp
        harnesses/fuzz_soap_router.cpp
        harnesses/fuzz_ws_discovery.cpp
        harnesses/fuzz_client_xml.cpp
        harnesses/fuzz_auth_headers.cpp
        harnesses/fuzz_url_validator.cpp
    )

    target_include_directories(TestFuzzRegression PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../services
        ${CMAKE_CURRENT_SOURCE_DIR}/../adapters
        ${CMAKE_SOURCE_DIR}/libs
    )

    target_compile_definitions(TestFuzzRegression PRIVATE
        FUZZ_STANDALONE_REGRESSION=1
        FUZZ_CORPUS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/corpus"
    )

    target_link_libraries(TestFuzzRegression PRIVATE
        Onvif
        PelcoDCore
        PelcoDSim
    )

    pelcod_register_gtest(TestFuzzRegression)
endif()
