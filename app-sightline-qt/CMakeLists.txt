add_executable(SightlineAppQt
    src/main.cpp
    src/SightlineQmlBridge.h
    src/SightlineQmlBridge.cpp
    src/SightlineVideoController.h
    src/SightlineVideoController.cpp
    src/TrackListModel.h
    src/TrackListModel.cpp
    src/TrafficLogModel.h
    src/TrafficLogModel.cpp
    src/VideoQuickItem.h
    src/VideoQuickItem.cpp
    qml.qrc
)

add_custom_target(app-sightline-qt DEPENDS SightlineAppQt)

target_include_directories(SightlineAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/Video
)

target_link_libraries(SightlineAppQt PRIVATE
    SightlineCore
    SightlineQt
    Video
    Transport
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
)

if(TARGET Qt6::QuickControls2)
    target_link_libraries(SightlineAppQt PRIVATE Qt6::QuickControls2)
endif()

apply_compiler_flags(SightlineAppQt)

if(BUILD_TESTING)
    add_executable(TestSightlineVideo
        src/VideoQuickItem.h
        src/VideoQuickItem.cpp
        src/SightlineVideoController.h
        src/SightlineVideoController.cpp
        src/TrackListModel.h
        src/TrackListModel.cpp
        src/TrafficLogModel.h
        src/TrafficLogModel.cpp
        src/SightlineQmlBridge.h
        src/SightlineQmlBridge.cpp
        tests/TestSightlineVideo.cpp
    )
    target_include_directories(TestSightlineVideo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_SOURCE_DIR}/libs/Video
    )
    target_link_libraries(TestSightlineVideo PRIVATE
        SightlineCore
        SightlineQt
        Transport
        Video
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Test
        GTest::gtest
    )
    apply_compiler_flags(TestSightlineVideo)
    pelcod_register_gtest(TestSightlineVideo NO_MAIN)
endif()

if(WIN32)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET SightlineAppQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:SightlineAppQt>"
            COMMENT "Deploying Qt libraries for SightlineAppQt using windeployqt..."
        )
    endif()

    find_file(VCPKG_APPLOCAL_SCRIPT
        NAMES applocal.ps1
        PATHS "C:/vcpkg/scripts/buildsystems/msbuild"
        NO_DEFAULT_PATH
    )
    if(VCPKG_APPLOCAL_SCRIPT)
        add_custom_command(TARGET SightlineAppQt POST_BUILD
            COMMAND powershell -ExecutionPolicy Bypass -File "${VCPKG_APPLOCAL_SCRIPT}"
                -targetBinary "$<TARGET_FILE:SightlineAppQt>"
                -installedDir "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin>"
            COMMENT "Deploying dependencies for SightlineAppQt using applocal.ps1..."
        )
        if(BUILD_TESTING AND TARGET TestSightlineVideo)
            add_custom_command(TARGET TestSightlineVideo POST_BUILD
                COMMAND powershell -ExecutionPolicy Bypass -File "${VCPKG_APPLOCAL_SCRIPT}"
                    -targetBinary "$<TARGET_FILE:TestSightlineVideo>"
                    -installedDir "$<IF:$<CONFIG:Debug>,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/debug/bin,${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin>"
                COMMENT "Deploying dependencies for TestSightlineVideo using applocal.ps1..."
            )
        endif()
    endif()
endif()
