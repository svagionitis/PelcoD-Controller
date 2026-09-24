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
    ${CMAKE_SOURCE_DIR}/libs/Mapping
    ${CMAKE_SOURCE_DIR}/libs/MappingQt
    ${CMAKE_SOURCE_DIR}/libs/Klv
)

target_link_libraries(VideoAppQt PRIVATE
    Video
    Mapping
    MappingQt
    Klv
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

if(WIN32)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET VideoAppQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:VideoAppQt>"
            COMMENT "Deploying Qt libraries for VideoAppQt using windeployqt..."
        )
    endif()

    find_file(VCPKG_APPLOCAL_SCRIPT
        NAMES applocal.ps1
        PATHS "C:/vcpkg/scripts/buildsystems/msbuild"
        NO_DEFAULT_PATH
    )
    if(VCPKG_APPLOCAL_SCRIPT)
        add_custom_command(TARGET VideoAppQt POST_BUILD
            COMMAND powershell -ExecutionPolicy Bypass -File "${VCPKG_APPLOCAL_SCRIPT}"
                -targetBinary "$<TARGET_FILE:VideoAppQt>"
                -installedDir "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin>"
            COMMENT "Deploying vcpkg runtime DLLs for VideoAppQt using applocal.ps1..."
        )
    endif()
endif()
