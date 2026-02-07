#include "platform.hpp"
#include "algo_pipeline/pipeline.hpp"
#include "hal/spi.hpp"
#include "hal/uart.hpp"
#include "hal/gpio.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <chrono>

static void run_application()
{
    using namespace system1;

    // Static allocation of resources (No Heap)
    hal::SpiMaster spi;
    hal::UartDevice uart;
    
    // Buffers for 70ms data interval
    std::array<std::uint8_t, 128> raw_data_buffer{};
    //std::array<std::uint8_t, algo::BleDataPipeline::MaxBleMtu> ble_payload_buffer{};

    while (true) {
        // 1. Acquire Data (Example via SPI)
        // In a real RTOS scenario, this might wait on a semaphore or task notification
        auto result = spi.transfer({}, raw_data_buffer);

        // 2. Process Data for BLE
        //auto process_res = algo::BleDataPipeline::process(raw_data_buffer, ble_payload_buffer);

        // 3. Transmit (Logic would go here)
        
        // 70ms cycle
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
    }
}

#if defined(IDF_TARGET)
// Entry point for ESP-IDF
extern "C" void app_main()
{
    run_application();
}
#else
// Entry point for Host (Windows, Linux)
int main()
{
    run_application();
    return 0;
}
#endif
