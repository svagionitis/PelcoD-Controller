add_executable(VideoAppQt
    main.cpp
    VideoQuickItem.h
    VideoQuickItem.cpp
    VideoPlayerController.h
    VideoPlayerController.cpp
    qml.qrc
)

add_custom_target(app-video-qt DEPENDS VideoAppQt)

target_include_directories(VideoAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/Video
    ${CMAKE_SOURCE_DIR}/libs/VideoFilters
)

target_link_libraries(VideoAppQt PRIVATE
    Video
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

if(PELCOD_ENABLE_FILTERS AND OpenCV_FOUND)
    target_link_libraries(VideoAppQt PRIVATE VideoFilters)
    target_compile_definitions(VideoAppQt PRIVATE PELCOD_HAS_FILTERS=1)
endif()

apply_compiler_flags(VideoAppQt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET VideoAppQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-compiler-runtime
            --no-translations
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            "$<TARGET_FILE:VideoAppQt>"
        COMMENT "Deploying Qt libraries for VideoAppQt using windeployqt..."
    )
endif()
