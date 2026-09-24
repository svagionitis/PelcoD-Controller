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
    StreamHealthOsdFilter.h
    StreamHealthOsdFilter.cpp
    TacticalHudFilter.h
    TacticalHudFilter.cpp
    MapRasterizerFilter.h
    MapRasterizerFilter.cpp
    VideoFilters.h
)

add_library(VideoFilters STATIC ${PELCOD_VIDEO_FILTERS_SOURCES})

target_include_directories(VideoFilters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(VideoFilters PUBLIC PELCOD_HAS_FILTERS)

target_include_directories(VideoFilters SYSTEM PUBLIC $<BUILD_INTERFACE:${OpenCV_INCLUDE_DIRS}>)

target_link_libraries(VideoFilters PUBLIC
    Video
    Math
    Klv
    Mapping
    Threads::Threads
    glog::glog
    opencv_core
    opencv_imgproc
    opencv_video
    opencv_calib3d
)

apply_compiler_flags(VideoFilters)
