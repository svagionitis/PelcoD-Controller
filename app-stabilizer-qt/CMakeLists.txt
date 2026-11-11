add_executable(StabilizerAppQt
    main.cpp
    RgbWarpUtility.h
    RgbWarpUtility.cpp
    StabilizerFramePacket.h
    StabilizerPlayerController.h
    StabilizerPlayerController.cpp
    StabilizerQuickItem.h
    StabilizerQuickItem.cpp
    StabilizerWorker.h
    StabilizerWorker.cpp
    SyntheticJitterGenerator.h
    SyntheticJitterGenerator.cpp
    qml.qrc
)

add_custom_target(app-stabilizer-qt DEPENDS StabilizerAppQt)

target_include_directories(StabilizerAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/Stabilizer
    ${CMAKE_SOURCE_DIR}/libs/Video
    ${CMAKE_SOURCE_DIR}/libs/Math
)

target_link_libraries(StabilizerAppQt PRIVATE
    Stabilizer
    Video
    Math
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

apply_compiler_flags(StabilizerAppQt)

if(WIN32)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET StabilizerAppQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:StabilizerAppQt>"
            COMMENT "Deploying Qt libraries for StabilizerAppQt using windeployqt..."
        )
    endif()
endif()

