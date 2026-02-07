// modules/hal/spi.hpp
#ifndef MODULES_HAL_SPI_HPP
#define MODULES_HAL_SPI_HPP

#include <cstdint>
#include <span>
#include <expected>

namespace system1::hal
{

enum class SpiError
{
    None,
    Timeout,
    BusError
};

/**
 * @brief Interface for SPI Master communication.
 * Implementation is selected by CMake based on the target platform.
 */
class SpiMaster
{
public:
    SpiMaster() = default;
    
    // Delete copy/move to enforce hardware uniqueness semantics
    SpiMaster(const SpiMaster&) = delete;
    SpiMaster& operator=(const SpiMaster&) = delete;

    /**
     * @brief Performs a full-duplex transfer.
     * @param tx_data Data to transmit.
     * @param rx_data Buffer to receive data. Must be same size as tx_data.
     */
    std::expected<void, SpiError> transfer(std::span<const std::uint8_t> tx_data, std::span<std::uint8_t> rx_data);
};

} // namespace system1::hal

#endif // MODULES_HAL_SPI_HPP
