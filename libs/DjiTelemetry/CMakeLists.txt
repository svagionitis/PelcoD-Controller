add_library(DjiTelemetry STATIC
    DjiSubtitleParser.h
    DjiSubtitleParser.cpp
    Mp4TextTrackReader.h
    Mp4TextTrackReader.cpp
    DjiSt0601Mapper.h
    DjiSt0601Mapper.cpp
    DjiTelemetrySource.h
    DjiTelemetrySource.cpp
)

target_include_directories(DjiTelemetry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# DjiTelemetry produces Klv::UasDatalinkMessage; Klv never depends on DjiTelemetry.
target_link_libraries(DjiTelemetry PUBLIC
    Klv
)

apply_compiler_flags(DjiTelemetry)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
