cmake_minimum_required(VERSION 3.30)

set(esp32_targets "esp32" "esp32c3" "esp32c6" "esp32p4")
set(TARGET "esp32c6" CACHE STRING "Specify the target ESP32 variant (e.g., esp32, esp32c3, esp32c6, esp32p4)")

if("${TARGET}" IN_LIST esp32_targets)
    # For ESP-IDF builds, the toolchain must be included BEFORE the project() call.
    # This ensures CMake uses the correct cross-compiler from the very beginning.
    include($ENV{IDF_PATH}/tools/cmake/idf.cmake)
endif()

# Now, with the toolchain configured, define the project.
project(system1 CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE
    Debug
        CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui, ccmake
    set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
            "Debug"
            "Release"
            "MinSizeRel"
            "RelWithDebInfo")
endif()

if(NOT ("${TARGET}" IN_LIST esp32_targets))
    message(STATUS "TARGET not specified or not an ESP32 target. Configuring for native host build.")
endif()

message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "CPU: ${CMAKE_SYSTEM_PROCESSOR}")

add_subdirectory(modules)
