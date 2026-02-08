#include "platform.hpp"
#include "data_processor.hpp"
#include "hal/logger.hpp"

#include <thread>
#include <chrono>

#if defined(IDF_TARGET)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#endif

using namespace system1;

static void run_application(app::Platform& platform)
{
    // Instantiate DataProcessor with the specific HAL interfaces
    // CTAD will deduce the template arguments from the constructor arguments
    app::DataProcessor processor(platform.spi(), platform.ble());

    hal::log(hal::LogLevel::Info, "Main", "Starting DataProcessor loop");

    // 70ms cycle time
    constexpr auto CycleTime = std::chrono::milliseconds(70);

#if defined(IDF_TARGET)
    // FreeRTOS precise timing initialization
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t cycle_ticks = pdMS_TO_TICKS(CycleTime.count());
#else
    // Host precise timing initialization
    auto next_wake = std::chrono::steady_clock::now();
#endif

    while (true) {
        // Execute one cycle of data acquisition, processing, and transmission
        processor.step();

        // Wait for the remainder of the cycle to maintain 70ms period without drift
#if defined(IDF_TARGET)
        vTaskDelayUntil(&last_wake_time, cycle_ticks);
#else
        next_wake += CycleTime;
        std::this_thread::sleep_until(next_wake);
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
    static app::Platform platform;
    run_application(platform);
}
#else
// Entry point for Host (Windows, Linux)
int main()
{
    static app::Platform platform;
    run_application(platform);
    return 0;
}
#endif
