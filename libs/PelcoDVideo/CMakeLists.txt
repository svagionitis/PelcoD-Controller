set(PELCOD_VIDEO_SOURCES
    DecoderTypes.h
    AtomicTripleBuffer.h
    IVideoDecoder.h
    MockVideoDecoder.h
    MockVideoDecoder.cpp
    DecoderFactory.h
    DecoderFactory.cpp
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

add_library(PelcoDVideo STATIC ${PELCOD_VIDEO_SOURCES})

target_include_directories(PelcoDVideo PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(PelcoDVideo PUBLIC
    glog::glog
)

if(PELCOD_ENABLE_FFMPEG AND FFMPEG_FOUND)
    target_compile_definitions(PelcoDVideo PUBLIC PELCOD_HAS_FFMPEG)
    target_include_directories(PelcoDVideo SYSTEM PUBLIC ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(PelcoDVideo PUBLIC ${FFMPEG_LIBRARIES})
endif()

if(PELCOD_ENABLE_GSTREAMER AND GSTREAMER_FOUND)
    target_compile_definitions(PelcoDVideo PUBLIC PELCOD_HAS_GSTREAMER)
    target_include_directories(PelcoDVideo SYSTEM PUBLIC ${GSTREAMER_INCLUDE_DIRS})
    target_link_libraries(PelcoDVideo PUBLIC ${GSTREAMER_LIBRARIES})
endif()

apply_compiler_flags(PelcoDVideo)
