#include "pipeline.hpp"
#include <algorithm>
#include <cstring>

namespace system1::algo
{

std::expected<std::size_t, PipelineError> BleDataPipeline::process(
    std::span<const std::uint8_t> input, 
    std::span<std::uint8_t> output
)
{
    // 1. Basic Input Validation
    if (input.empty())
    {
        return 0; // Nothing to process
    }

    // Ensure output buffer is sufficient
    if (output.size() < input.size())
    {
        return std::unexpected(PipelineError::OutputBufferTooSmall);
    }

    // 2. Data Integrity Check (Example)
    // In a real scenario, we might check for a specific header or checksum here.
    // For now, we filter out packets that are completely zeroed, which might 
    // happen if the SPI bus was idle or disconnected.
    bool all_zeros = std::all_of(input.begin(), input.end(), [](uint8_t b){ return b == 0; });
    if (all_zeros) {
        return 0; // Treat as no valid data, do not send
    }

    // 3. Processing / Transformation
    // Currently, we perform a direct pass-through (transparent bridge).
    // Future expansion: Add sequence numbers, timestamps, or compress data here.
    std::copy(input.begin(), input.end(), output.begin());

    // Return the size of the payload to be sent
    return input.size();
}

} // namespace system1::algo
