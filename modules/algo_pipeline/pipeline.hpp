#ifndef MODULES_ALGO_PIPELINE_PIPELINE_HPP
#define MODULES_ALGO_PIPELINE_PIPELINE_HPP

#include <cstdint>
#include <span>
#include <expected>

namespace system1::algo
{

enum class PipelineError
{
    InvalidInput,
    OutputBufferTooSmall,
    ProcessingFailed
};

/**
 * @brief Stateless pipeline for processing raw system data into BLE payloads.
 */
class BleDataPipeline
{
public:
    // Maximum BLE MTU size (Attribute Protocol MTU).
    // Typical max for BLE 4.2/5.0 is 247 bytes (251 - 4 bytes L2CAP header).
    // We define this constant to size buffers statically.
    static constexpr std::size_t MaxBleMtu = 247; 

    /**
     * @brief Process raw data from System2 and prepare it for BLE transmission.
     * 
     * Validates the input data and formats it into the output buffer.
     * 
     * @param input Raw data received from System2 (e.g., via SPI/UART).
     * @param output Buffer to store the processed BLE payload. Must be at least input.size().
     * @param sensitivity Sensitivity threshold for the algorithm (0-255).
     * @return std::expected<std::size_t, PipelineError> Number of bytes written to output on success.
     */
    static std::expected<std::size_t, PipelineError> process(
        std::span<const std::uint8_t> input, 
        std::span<std::uint8_t> output,
        std::uint8_t sensitivity
    );
};

} // namespace system1::algo

#endif // MODULES_ALGO_PIPELINE_PIPELINE_HPP
