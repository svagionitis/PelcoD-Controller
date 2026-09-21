# ==============================================================================
# PelcoDTestSupport Interface Target
# ==============================================================================

add_library(PelcoDTestSupport INTERFACE)

target_include_directories(PelcoDTestSupport INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/PelcoDTransport
)

target_link_libraries(PelcoDTestSupport INTERFACE
    PelcoDCore
    PelcoDTransport
    GTest::gtest
    GTest::gmock
)
