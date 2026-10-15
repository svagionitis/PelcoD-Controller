add_executable(OnvifAppQt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    tabs/OnvifCameraTab.h
    tabs/OnvifCameraTab.cpp
    tabs/OnvifServerTab.h
    tabs/OnvifServerTab.cpp
    resources.qrc
)

target_include_directories(OnvifAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/Onvif
)

target_link_libraries(OnvifAppQt PRIVATE
    PelcoDQt
    Onvif
    Qt6::Widgets
)

apply_compiler_flags(OnvifAppQt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET OnvifAppQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:OnvifAppQt>"
        COMMENT "Deploying Qt libraries for OnvifAppQt using windeployqt..."
    )
endif()
