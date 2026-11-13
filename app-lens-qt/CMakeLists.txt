add_executable(LensAppQt
    main.cpp
    LensConfigModel.h
    LensConfigModel.cpp
    LensPlayerController.h
    LensPlayerController.cpp
    LensVideoQuickItem.h
    LensVideoQuickItem.cpp
    LensWorker.h
    LensWorker.cpp
    PresentationTypes.h
    qml.qrc
)

add_custom_target(app-lens-qt DEPENDS LensAppQt)

target_include_directories(LensAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/LensController
    ${CMAKE_SOURCE_DIR}/libs/Video
    ${CMAKE_SOURCE_DIR}/libs/Math
    ${CMAKE_SOURCE_DIR}/libs/Transport
)

target_link_libraries(LensAppQt PRIVATE
    LensController
    Video
    Math
    Transport
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

apply_compiler_flags(LensAppQt)

option(PELCOD_DEPLOY_QT "Deploy Qt libraries using windeployqt" OFF)

if(WIN32 AND PELCOD_DEPLOY_QT)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET LensAppQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                "$<TARGET_FILE:LensAppQt>"
            COMMENT "Deploying Qt libraries for LensAppQt using windeployqt..."
        )
    endif()
endif()

