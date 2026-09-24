add_library(Mapping STATIC
    GeoTypes.h
    MercatorProjection.h
    MercatorProjection.cpp
    MapViewport.h
    MapViewport.cpp
    ITileProvider.h
    TileCache.h
    TileCache.cpp
    DiskTileCache.h
    DiskTileCache.cpp
    ProceduralGridTileProvider.h
    ProceduralGridTileProvider.cpp
    CompositeTileProvider.h
    CompositeTileProvider.cpp
    TacticalOverlay.h
    TacticalOverlay.cpp
)

target_include_directories(Mapping PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Mapping PUBLIC
    Klv
    Threads::Threads
    glog::glog
)

apply_compiler_flags(Mapping)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
