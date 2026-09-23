add_library(Klv STATIC
    KlvTypes.h
    KlvBer.h
    KlvBer.cpp
    KlvCrc.h
    KlvCrc.cpp
    KlvEncoder.h
    KlvEncoder.cpp
    KlvParser.h
    KlvParser.cpp
    KlvStreamScanner.h
    KlvStreamScanner.cpp
    MpegTsKlvExtractor.h
    MpegTsKlvExtractor.cpp
    KlvGeodesy.h
    KlvGeodesy.cpp
)

target_include_directories(Klv PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Klv PUBLIC
    Threads::Threads
    glog::glog
)

apply_compiler_flags(Klv)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
