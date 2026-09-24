# ==============================================================================
# MappingQt Unit Tests
# ==============================================================================

function(add_mapping_qt_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    target_link_libraries(${test_name} PRIVATE
        MappingQt
        Mapping
        Klv
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
    )
    pelcod_register_gtest(${test_name} NO_MAIN)

    if(WIN32 AND TARGET VideoAppQt)
        add_custom_command(TARGET ${test_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE_DIR:VideoAppQt>/Qt6Qmld.dll"
                "$<TARGET_FILE_DIR:VideoAppQt>/Qt6Quickd.dll"
                "$<TARGET_FILE_DIR:VideoAppQt>/Qt6Networkd.dll"
                "$<TARGET_FILE_DIR:VideoAppQt>/Qt6OpenGLd.dll"
                "$<TARGET_FILE_DIR:VideoAppQt>/Qt6QmlModelsd.dll"
                "$<TARGET_FILE_DIR:${test_name}>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_FILE_DIR:VideoAppQt>/platforms"
                "$<TARGET_FILE_DIR:${test_name}>/platforms"
            COMMENT "Copying Qt Qml/Quick dependencies for ${test_name}"
        )
    endif()
endfunction()

add_mapping_qt_test(TestTacticalMapQuickItem TestTacticalMapQuickItem.cpp)
