#ifndef APP_DATA_PROCESSOR_HPP
#define APP_DATA_PROCESSOR_HPP

#include "algo_pipeline/pipeline.hpp"
#include <array>
#include <span>
#include <cstdint>
#include <concepts>
#include <optional>
#include "hal/spi.hpp"
#include "hal/ble.hpp"
#include "hal/logger.hpp"

namespace system1::app
{

// Concept for SPI Driver
template<typename T>
concept SpiDriverConcept = requires(T& t, std::span<const std::uint8_t> tx, std::span<std::uint8_t> rx) {
    { t.transfer(tx, rx) } -> std::convertible_to<std::expected<void, hal::SpiError>>;
};

// Concept for BLE Driver
template<typename T>
concept BleDriverConcept = requires(T& t, std::span<const std::uint8_t> data) {
    { t.send(data) } -> std::convertible_to<std::expected<void, hal::BleError>>;
    { t.get_pending_settings() } -> std::same_as<std::optional<hal::DeviceSettings>>;
};

/**
 * @brief Orchestrates data acquisition, processing, and transmission.
 * 
 * This class is templated on the HAL interfaces (SpiDriver, BleDriver) to allow
 * for dependency injection. This enables unit testing with mocks and zero-overhead
 * static polymorphism for the target build.
 */
template <SpiDriverConcept SpiDriver, BleDriverConcept BleDriver>
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
        // 0. Check for settings updates
        if (auto settings = ble_.get_pending_settings()) {
            current_settings_ = *settings;
            hal::log(hal::LogLevel::Info, "DataProcessor", "Settings updated: sensitivity={}", static_cast<int>(current_settings_.sensitivity));
        }

        // 1. Acquire Data
        // Buffer for raw data from System2
        std::array<std::uint8_t, 128> raw_data_buffer;
        
        // Perform SPI transfer (Receive only)
        auto spi_result = spi_.transfer({}, raw_data_buffer);
        
        if (!spi_result) {
            hal::log(hal::LogLevel::Error, "DataProcessor", "SPI transfer failed");
            return;
        }

        raw_data_buffer.at(0) = 101;
        // 2. Process Data
        std::array<std::uint8_t, algo::BleDataPipeline::MaxBleMtu> ble_payload_buffer;
        auto process_result = algo::BleDataPipeline::process(raw_data_buffer, ble_payload_buffer, current_settings_.sensitivity);

        if (!process_result) {
            hal::log(hal::LogLevel::Error, "DataProcessor", "Pipeline processing error");
            return;
        }

        // 3. Transmit if we have valid data
        if (*process_result > 0) {
            // Create a span of the valid data portion and send it
            auto payload_span = std::span(ble_payload_buffer).first(*process_result);
            if (auto result = ble_.send(payload_span); !result) {
                hal::log(hal::LogLevel::Warning, "DataProcessor", "BLE send failed");
            }
        }
    }

private:
    SpiDriver& spi_;
    BleDriver& ble_;
    hal::DeviceSettings current_settings_{ .sensitivity = 50, .led_brightness = 128, .reporting_interval_ms = 70 };
};

} // namespace system1::app

#endif // APP_DATA_PROCESSOR_HPP
