# ==============================================================================
# ONVIF Profile S Conformance Runner & Server Daemon Target
# ==============================================================================

add_executable(OnvifServerApp
    OnvifServerApp.cpp
)

target_include_directories(OnvifServerApp PRIVATE
    ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(OnvifServerApp PRIVATE
    Onvif
    PelcoDCore
    Threads::Threads
)

if(WIN32)
    target_link_libraries(OnvifServerApp PRIVATE ws2_32)
endif()

apply_compiler_flags(OnvifServerApp)

find_package(Python3 COMPONENTS Interpreter QUIET)

if(Python3_FOUND)
    add_custom_target(onvif_conformance
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_conformance.py
                --server-binary $<TARGET_FILE:OnvifServerApp>
                --output-junit ${CMAKE_BINARY_DIR}/conformance_report.xml
        DEPENDS OnvifServerApp
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Executing ONVIF Profile S Conformance Runner with XML Schema Validation"
    )

    if(BUILD_TESTING)
        add_test(
            NAME OnvifConformanceTest
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_conformance.py
                    --server-binary $<TARGET_FILE:OnvifServerApp>
                    --output-junit ${CMAKE_BINARY_DIR}/conformance_report.xml
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        set_tests_properties(OnvifConformanceTest PROPERTIES TIMEOUT 180)
    endif()
endif()
