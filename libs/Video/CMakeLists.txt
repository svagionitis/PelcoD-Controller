set(PELCOD_VIDEO_SOURCES
    DecoderTypes.h
    AtomicTripleBuffer.h
    IVideoDecoder.h
    BaseVideoDecoder.h
    BaseVideoDecoder.cpp
    MockVideoDecoder.h
    MockVideoDecoder.cpp
    DecoderFactory.h
    DecoderFactory.cpp
    DeviceEnumerator.h
    DeviceEnumerator.cpp
    BrailleRenderer.h
    BrailleRenderer.cpp
)

if(PELCOD_ENABLE_FFMPEG AND FFMPEG_FOUND)
    list(APPEND PELCOD_VIDEO_SOURCES
        FFmpegDecoder.h
        FFmpegDecoder.cpp
    )
endif()

if(PELCOD_ENABLE_GSTREAMER AND GSTREAMER_FOUND)
    list(APPEND PELCOD_VIDEO_SOURCES
        GStreamerDecoder.h
        GStreamerDecoder.cpp
    )
endif()

add_library(Video STATIC ${PELCOD_VIDEO_SOURCES})

target_include_directories(Video PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Video PUBLIC
    glog::glog
)

if(WIN32)
    target_link_libraries(Video PUBLIC ole32 oleaut32 strmiids)
endif()

if(PELCOD_ENABLE_FFMPEG AND FFMPEG_FOUND)
    target_compile_definitions(Video PUBLIC PELCOD_HAS_FFMPEG)
    target_include_directories(Video SYSTEM PUBLIC ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(Video PUBLIC ${FFMPEG_LIBRARIES})
endif()

if(PELCOD_ENABLE_GSTREAMER AND GSTREAMER_FOUND)
    target_compile_definitions(Video PUBLIC PELCOD_HAS_GSTREAMER)
    target_include_directories(Video SYSTEM PUBLIC ${GSTREAMER_INCLUDE_DIRS})
    target_link_libraries(Video PUBLIC ${GSTREAMER_LIBRARIES})
endif()

apply_compiler_flags(Video)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
