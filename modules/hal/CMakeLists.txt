add_library(hal)

# Include the parent directory to allow #include "hal/..." usage
target_include_directories(hal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if("${TARGET}" IN_LIST esp32_targets)
    # ESP32-C6 Implementation
    target_sources(hal PRIVATE
        src/esp32/spi.cpp
        src/esp32/uart.cpp
        src/esp32/gpio.cpp
        src/esp32/logger.cpp
        src/esp32/ble.cpp
    )
    
    # Link against ESP-IDF drivers
    target_link_libraries(hal PRIVATE 
        idf::driver 
        idf::esp_driver_gpio 
        idf::esp_driver_spi 
        idf::esp_driver_uart
        idf::bt
    )
else()
    # Host (FT232H) Implementation
    target_sources(hal PRIVATE
        src/host/spi.cpp
        src/host/uart.cpp
        src/host/gpio.cpp
        src/host/logger.cpp
        src/host/ble.cpp
    )
    
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FTDI REQUIRED libftdi1)
    target_include_directories(hal PRIVATE ${FTDI_INCLUDE_DIRS})
    target_link_libraries(hal PRIVATE ${FTDI_LIBRARIES})
endif()