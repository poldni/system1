#include "platform.hpp"
#include "data_processor.hpp"
#include "hal/spi.hpp"
#include "hal/uart.hpp"
#include "hal/gpio.hpp"
#include "hal/ble.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <chrono>

#if defined(IDF_TARGET)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#endif

static void run_application(system1::app::Platform& platform)
{
    // Instantiate DataProcessor with the specific HAL interfaces
    // CTAD will deduce the template arguments from the constructor arguments
    system1::app::DataProcessor processor(platform.spi(), platform.ble());

    while (true) {
        // Execute one cycle of data acquisition, processing, and transmission
        processor.step();

        // 70ms cycle
#if defined(IDF_TARGET)
        vTaskDelay(pdMS_TO_TICKS(70));
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
#endif
    }
}

#if defined(IDF_TARGET)
// Entry point for ESP-IDF
extern "C" void app_main()
{
    // Initialize NVS (Required for Bluetooth)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Platform initialization (HAL drivers)
    static system1::app::Platform platform;
    run_application(platform);
}
#else
// Entry point for Host (Windows, Linux)
int main()
{
    system1::app::Platform platform;
    run_application(platform);
    return 0;
}
#endif
