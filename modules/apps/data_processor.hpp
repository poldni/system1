#ifndef APP_DATA_PROCESSOR_HPP
#define APP_DATA_PROCESSOR_HPP

#include "algo_pipeline/pipeline.hpp"
#include <array>
#include <span>
#include <cstdint>

namespace system1::app
{

/**
 * @brief Orchestrates data acquisition, processing, and transmission.
 * 
 * This class is templated on the HAL interfaces (SpiDriver, BleDriver) to allow
 * for dependency injection. This enables unit testing with mocks and zero-overhead
 * static polymorphism for the target build.
 */
template <typename SpiDriver, typename BleDriver>
class DataProcessor
{
public:
    DataProcessor(SpiDriver& spi, BleDriver& ble)
        : spi_(spi)
        , ble_(ble)
    {
    }

    /**
     * @brief Execute one cycle of the data processing loop.
     * 1. Acquire data from SPI.
     * 2. Process data using the algorithm pipeline.
     * 3. Send valid data via BLE.
     */
    void step()
    {
        // 1. Acquire Data
        // Buffer for raw data from System2
        std::array<std::uint8_t, 128> raw_data_buffer;
        
        // Perform SPI transfer (Receive only)
        auto spi_result = spi_.transfer({}, raw_data_buffer);
        
        if (!spi_result) {
            // In a real app, we might log the error here
            return;
        }

        // 2. Process Data
        std::array<std::uint8_t, algo::BleDataPipeline::MaxBleMtu> ble_payload_buffer;
        auto process_result = algo::BleDataPipeline::process(raw_data_buffer, ble_payload_buffer);

        // 3. Transmit if we have valid data
        if (process_result && *process_result > 0) {
            // Create a span of the valid data portion and send it
            auto payload_span = std::span(ble_payload_buffer).first(*process_result);
            ble_.send(payload_span);
        }
    }

private:
    SpiDriver& spi_;
    BleDriver& ble_;
};

} // namespace system1::app

#endif // APP_DATA_PROCESSOR_HPP
