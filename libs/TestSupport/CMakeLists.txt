# ==============================================================================
# PelcoDTestSupport Interface Target
# ==============================================================================

add_library(PelcoDTestSupport INTERFACE)

target_include_directories(PelcoDTestSupport INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
    ${CMAKE_SOURCE_DIR}/libs/Transport
)

target_link_libraries(PelcoDTestSupport INTERFACE
    PelcoDCore
    Transport
    GTest::gtest
    GTest::gmock
)
