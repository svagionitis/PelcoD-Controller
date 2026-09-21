# ==============================================================================
# PelcoDSim Unit Tests
# ==============================================================================

set(PELCOD_TEST_SUPPORT_DIR "${CMAKE_SOURCE_DIR}/libs/TestSupport")

function(add_sim_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/PelcoDCore
        ${PELCOD_TEST_SUPPORT_DIR}
    )
    target_link_libraries(${test_name} PRIVATE
        PelcoDSim
        PelcoDCore
        Threads::Threads
    )
    pelcod_register_gtest(${test_name})
endfunction()

add_sim_test(TestKinematicsSimulator TestKinematicsSimulator.cpp)
add_sim_test(TestLatencyPipeline     TestLatencyPipeline.cpp)
