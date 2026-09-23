set(PELCOD_VIDEO_FILTERS_SOURCES
    ColorFilters.h
    ColorFilters.cpp
    SpatialFilters.h
    SpatialFilters.cpp
    GeometricFilters.h
    GeometricFilters.cpp
    ThermalFilters.h
    ThermalFilters.cpp
    TrackingFilters.h
    TrackingFilters.cpp
    OverlayFilters.h
    OverlayFilters.cpp
    VideoFilters.h
)

add_library(VideoFilters STATIC ${PELCOD_VIDEO_FILTERS_SOURCES})

target_include_directories(VideoFilters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(VideoFilters PUBLIC PELCOD_HAS_FILTERS)
if(MSVC)
    target_compile_definitions(VideoFilters PUBLIC CV_IGNORE_DEBUG_BUILD_GUARD)
endif()

target_include_directories(VideoFilters SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})

target_link_libraries(VideoFilters PUBLIC
    Video
    Math
    Threads::Threads
    glog::glog
    ${OpenCV_LIBS}
)

apply_compiler_flags(VideoFilters)
