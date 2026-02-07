
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# --- Application Executable ---
add_executable(system1_app apps/main.cpp)

# Add the current directory (modules) to the include path for all targets below it.
# This allows for includes like "hal/gpio.hpp" and "common/span.hpp".
target_include_directories(system1_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(algo_pipeline)
add_subdirectory(hal)

# The application links to the platform-agnostic logic library
target_link_libraries(system1_app PRIVATE algo_pipeline hal)

if("${TARGET}" IN_LIST esp32_targets)
   
    idf_build_process("${TARGET}"
                    # try and trim the build; additional components
                    # will be included as needed based on dependency tree
                    #
                    # although esptool_py does not generate static library,
                    # processing the component is needed for flashing related
                    # targets and file generation
                    COMPONENTS freertos driver esptool_py
                    SDKCONFIG ${CMAKE_SOURCE_DIR}/sdkconfig
                    BUILD_DIR ${CMAKE_BINARY_DIR})

    # Create a BSP library for ESP32. This library contains all
    # platform-specific code, definitions, and dependencies.
    # We use an INTERFACE library here assuming the BSP consists of header-only template implementations.
    # If you add .cpp files to the BSP, change this to a STATIC library.
    add_library(bsp_esp32 INTERFACE)
    target_compile_definitions(bsp_esp32 INTERFACE -DIDF_TARGET="${TARGET}")
    target_link_libraries(bsp_esp32 INTERFACE idf::esp_driver_spi
                                            idf::esp_driver_uart
                                            idf::esp_driver_gpio 
                                            idf::esp_driver_ledc
                                            idf::esp_hw_support 
                                            idf::log 
                                            idf::freertos)

    # Explicitly add the definition to the application target. This is often
    # needed to help IDEs and IntelliSense engines recognize the definition.
    target_compile_definitions(system1_app PRIVATE -DIDF_TARGET="${TARGET}")
    # The main application links to the specific BSP for this target
    target_link_libraries(system1_app PRIVATE bsp_esp32)

    # This ESP-IDF function handles component dependencies, final linking, flashing, etc.
    idf_build_executable(system1_app)
endif()
