add_executable(RtspServerAppQt
    main.cpp
    RtspServerController.h
    RtspServerController.cpp
    RtspStreamFeeder.h
    RtspStreamFeeder.cpp
    ServerPresetManager.h
    ServerPresetManager.cpp
    qml.qrc
)

add_custom_target(app-rtsp-server-qt DEPENDS RtspServerAppQt)

target_include_directories(RtspServerAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/Video
)

target_link_libraries(RtspServerAppQt PRIVATE
    Video
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    glog::glog
)

apply_compiler_flags(RtspServerAppQt)

if(UNIX AND NOT APPLE)
    set_target_properties(RtspServerAppQt PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
        BUILD_RPATH "${CMAKE_PREFIX_PATH}/lib"
    )
endif()

if(WIN32)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET RtspServerAppQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:RtspServerAppQt>"
            COMMENT "Deploying Qt libraries for RtspServerAppQt using windeployqt..."
        )
    endif()
endif()

if(BUILD_TESTING)
    add_executable(TestRtspServerApp
        RtspServerController.h
        RtspServerController.cpp
        RtspStreamFeeder.h
        RtspStreamFeeder.cpp
        ServerPresetManager.h
        ServerPresetManager.cpp
        tests/TestRtspServerController.cpp
        tests/TestRtspStreamFeeder.cpp
    )

    target_include_directories(TestRtspServerApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/libs/Video
    )

    target_link_libraries(TestRtspServerApp PRIVATE
        Video
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        GTest::gtest
        GTest::gtest_main
        glog::glog
    )

    apply_compiler_flags(TestRtspServerApp)

    if(UNIX AND NOT APPLE)
        set_target_properties(TestRtspServerApp PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH TRUE
            BUILD_RPATH "${CMAKE_PREFIX_PATH}/lib"
        )
    endif()

    pelcod_register_gtest(TestRtspServerApp)
endif()
