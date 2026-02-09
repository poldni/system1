# --- Application Executable ---
add_executable(system1_app main.cpp)

# Add the 'modules' directory to include path to find "hal/...", "algo_pipeline/..."
target_include_directories(system1_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Link against the logic libraries
target_link_libraries(system1_app PRIVATE algo_pipeline hal)

# If the ESP32 BSP target exists (defined in parent), link against it
if(TARGET bsp_esp32)
    target_link_libraries(system1_app PRIVATE bsp_esp32)
endif()

# --- Test Executable ---

# Build tests only if not targeting ESP32 (Host build)
if(NOT "${TARGET}" IN_LIST esp32_targets)
    find_package(GTest)
    
    if(GTest_FOUND)
        add_executable(system1_test 
            test/test_data_processor.cpp
        )

        # Include 'modules' to find "apps/...", "hal/..."
        target_include_directories(system1_test PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/..
        )

        # Link against the algorithm pipeline (real implementation)
        # We mock HAL, so we don't link 'hal' library.
        target_link_libraries(system1_test PRIVATE 
            algo_pipeline
            GTest::gtest 
            GTest::gmock 
            GTest::gtest_main
        )

        include(GoogleTest)
        gtest_discover_tests(system1_test)
    else()
        message(STATUS "GTest not found, skipping system1_test")
    endif()
endif()