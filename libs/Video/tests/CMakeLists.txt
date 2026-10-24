# ==============================================================================
# PelcoDVideo Unit Tests  (requires PELCOD_ENABLE_VIDEO)
# ==============================================================================

if(PELCOD_ENABLE_QT AND TARGET PelcoDQt AND TARGET Qt6::Test)
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
        Klv
        Qt6::Core
        Qt6::Gui
        Qt6::Test
    )
    if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
        target_link_libraries(TestVideoDecoder PRIVATE VideoFilters)
    endif()
    pelcod_register_gtest(TestVideoDecoder NO_MAIN)
endif()

add_executable(TestStreamHealthMonitor TestStreamHealthMonitor.cpp)
target_include_directories(TestStreamHealthMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(TestStreamHealthMonitor PRIVATE
    Video
    GTest::gtest_main
)
pelcod_register_gtest(TestStreamHealthMonitor)

add_executable(TestLatencyTracker TestLatencyTracker.cpp)
target_include_directories(TestLatencyTracker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(TestLatencyTracker PRIVATE
    Video
    GTest::gtest_main
)
pelcod_register_gtest(TestLatencyTracker)

add_executable(TestMpegTsH264Demuxer TestMpegTsH264Demuxer.cpp)
target_include_directories(TestMpegTsH264Demuxer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(TestMpegTsH264Demuxer PRIVATE
    Video
    GTest::gtest_main
)
pelcod_register_gtest(TestMpegTsH264Demuxer)

add_executable(TestLive555FrameQueue TestLive555FrameQueue.cpp)
target_include_directories(TestLive555FrameQueue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(TestLive555FrameQueue PRIVATE
    Video
    GTest::gtest_main
)
pelcod_register_gtest(TestLive555FrameQueue)

if(PELCOD_ENABLE_LIVE555 AND LIVE555_FOUND AND OPENH264_FOUND)
    add_executable(TestLive555Decoder TestLive555Decoder.cpp)
    target_include_directories(TestLive555Decoder PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TestLive555Decoder PRIVATE
        Video
        GTest::gtest_main
    )
    pelcod_register_gtest(TestLive555Decoder)

    add_executable(TestLive555Sources TestLive555Sources.cpp)
    target_include_directories(TestLive555Sources PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TestLive555Sources PRIVATE
        Video
        GTest::gtest_main
    )
    pelcod_register_gtest(TestLive555Sources)

    add_executable(TestLive555Subsession TestLive555Subsession.cpp)
    target_include_directories(TestLive555Subsession PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TestLive555Subsession PRIVATE
        Video
        GTest::gtest_main
    )
    pelcod_register_gtest(TestLive555Subsession)

    add_executable(TestLive555RtspServer TestLive555RtspServer.cpp)
    target_include_directories(TestLive555RtspServer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(TestLive555RtspServer PRIVATE
        Video
        GTest::gtest_main
    )
    pelcod_register_gtest(TestLive555RtspServer)
endif()

if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    add_executable(TestMapRasterizerFilter TestMapRasterizerFilter.cpp)
    target_include_directories(TestMapRasterizerFilter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/VideoFilters
        ${CMAKE_SOURCE_DIR}/libs/Mapping
        ${CMAKE_SOURCE_DIR}/libs/Klv
    )
    target_link_libraries(TestMapRasterizerFilter PRIVATE
        VideoFilters
        Video
        Mapping
        Klv
        opencv_core
        opencv_imgproc
    )
    pelcod_register_gtest(TestMapRasterizerFilter)

    add_executable(TestTacticalHudFilter TestTacticalHudFilter.cpp)
    target_include_directories(TestTacticalHudFilter PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/VideoFilters
        ${CMAKE_SOURCE_DIR}/libs/Klv
    )
    target_link_libraries(TestTacticalHudFilter PRIVATE
        VideoFilters
        Video
        Klv
        opencv_core
        opencv_imgproc
    )
    pelcod_register_gtest(TestTacticalHudFilter)
endif()
