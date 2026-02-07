// modules/hal/uart.hpp
#ifndef MODULES_HAL_UART_HPP
#define MODULES_HAL_UART_HPP

#include <cstdint>
#include <span>
#include <expected>

namespace system1::hal
{

enum class UartError
{
    None,
    Timeout,
    ReadError,
    WriteError
};

/**
 * @brief Interface for UART communication.
 */
class UartDevice
{
public:
    UartDevice() = default;
    
    UartDevice(const UartDevice&) = delete;
    UartDevice& operator=(const UartDevice&) = delete;

    /**
     * @brief Reads data into the buffer.
     * @return Number of bytes read.
     */
    std::expected<std::size_t, UartError> read(std::span<std::uint8_t> buffer);

    /**
     * @brief Writes data to the UART.
     */
    std::expected<void, UartError> write(std::span<const std::uint8_t> data);
};

} // namespace system1::hal

#endif // MODULES_HAL_UART_HPP
