# ==============================================================================
# PelcoDVideo Unit Tests  (requires PELCOD_ENABLE_VIDEO)
# ==============================================================================

add_executable(TestVideoDecoder TestVideoDecoder.cpp)
target_include_directories(TestVideoDecoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs/VideoFilters
    ${CMAKE_SOURCE_DIR}/libs/PelcoDQt
    ${CMAKE_SOURCE_DIR}/libs/Klv
)
target_link_libraries(TestVideoDecoder PRIVATE
    PelcoDQt
    Video
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)
if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    target_link_libraries(TestVideoDecoder PRIVATE VideoFilters)
endif()
pelcod_register_gtest(TestVideoDecoder NO_MAIN)

add_executable(TestStreamHealthMonitor TestStreamHealthMonitor.cpp)
target_include_directories(TestStreamHealthMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(TestStreamHealthMonitor PRIVATE
    Video
    GTest::gtest_main
)
pelcod_register_gtest(TestStreamHealthMonitor)
