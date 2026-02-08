#include "pipeline.hpp"
#include <algorithm>
#include <cstring>

namespace system1::algo
{

std::expected<std::size_t, PipelineError> BleDataPipeline::process(
    std::span<const std::uint8_t> input, 
    std::span<std::uint8_t> output,
    std::uint8_t sensitivity
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

    // 2. Data Integrity / Sensitivity Check
    // Filter out packets where the strongest signal (max byte value)
    // does not exceed the sensitivity threshold.
    uint8_t max_val = 0;
    for (const auto b : input) {
        if (b > max_val) max_val = b;
    }

    if (max_val == 0 || max_val < sensitivity) {
        return 0; // Signal too weak or empty, do not send
    }

    // 3. Processing / Transformation
    // Currently, we perform a direct pass-through (transparent bridge).
    // Future expansion: Add sequence numbers, timestamps, or compress data here.
    std::copy(input.begin(), input.end(), output.begin());

    // Return the size of the payload to be sent
    return input.size();
}

} // namespace system1::algo
