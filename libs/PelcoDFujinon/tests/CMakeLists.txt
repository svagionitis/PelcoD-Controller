# ==============================================================================
# PelcoDFujinon Unit Tests
# ==============================================================================

function(add_fujinon_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
        ${CMAKE_SOURCE_DIR}/libs/PelcoDSim
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDFujinon
        PelcoDSim
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_fujinon_test(TestFujinonProtocol    TestFujinonProtocol.cpp)
add_fujinon_test(TestFujinonSX800Device TestFujinonSX800Device.cpp)
