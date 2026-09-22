add_executable(ViscaAppQt
    main.cpp
    MainWindow.h
    MainWindow.cpp
    QViscaSonyDevice.h
    QViscaSonyDevice.cpp
    QViscaBusScanner.h
    QViscaBusScanner.cpp
    widgets/ViscaConnectionWidget.h
    widgets/ViscaConnectionWidget.cpp
    widgets/ViscaTrafficInspectorWidget.h
    widgets/ViscaTrafficInspectorWidget.cpp
    dialogs/ViscaBusScanDialog.h
    dialogs/ViscaBusScanDialog.cpp
    tabs/LensOpticsTab.h
    tabs/LensOpticsTab.cpp
    tabs/ExposureImagingTab.h
    tabs/ExposureImagingTab.cpp
    tabs/RegistersHardwareTab.h
    tabs/RegistersHardwareTab.cpp
    tabs/SystemDiagnosticsTab.h
    tabs/SystemDiagnosticsTab.cpp
    resources.qrc
)

target_include_directories(ViscaAppQt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/ViscaCore
    ${CMAKE_SOURCE_DIR}/libs/ViscaSony
    ${CMAKE_SOURCE_DIR}/libs/Transport
)

target_link_libraries(ViscaAppQt PRIVATE
    ViscaSony
    ViscaCore
    Transport
    Qt6::Widgets
    glog::glog
)

apply_compiler_flags(ViscaAppQt)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET ViscaAppQt POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-compiler-runtime
            --no-translations
            "$<TARGET_FILE:ViscaAppQt>"
        COMMENT "Deploying Qt libraries for ViscaAppQt using windeployqt..."
    )
endif()
