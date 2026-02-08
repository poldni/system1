#include "hal/uart.hpp"
#include "ftdi_context.hpp"
#include <array>
#include <algorithm>

namespace system1::hal
{

namespace {
    // Mapping UART TX to ACBUS0 (Pin 8 in GPIO abstraction)
    // ACBUS0 is the LSB of the High Byte in MPSSE GPIO commands
    constexpr uint8_t TX_PIN_MASK = 0x01; 
    
    // Baud rate calculation for MPSSE
    // Clock = 60MHz
    // Baud = 60MHz / ((1 + Divisor) * 2)
    // Divisor = (30MHz / Baud) - 1
    // For 115200: (30000000 / 115200) - 1 = 259.41 -> 259
    // Actual: 60M / 520 = 115384 Hz (0.16% error)
    constexpr uint16_t DIVISOR_115200 = 259; 
    constexpr uint16_t DIVISOR_1MHZ = 29;    // Default for SPI
}

std::expected<std::size_t, UartError> UartDevice::read(std::span<std::uint8_t> buffer)
{
    // RX via MPSSE bit-banging on a Host OS is not feasible for 115200 baud
    // due to USB round-trip latency (~1ms) being much larger than a bit time (8.6us).
    // We cannot sample the start bit in real-time.
    // For this architecture, we assume UART RX is not supported in this mode.
    return 0;
}

std::expected<void, UartError> UartDevice::write(std::span<const std::uint8_t> data)
{
    auto& dev = get_ftdi_instance();
    if (!dev.ensure_connected()) {
        return std::unexpected(UartError::WriteError);
    }

    if (data.empty()) return {};

    // Configure ACBUS0 as Output
    dev.acbus_dir |= TX_PIN_MASK;
    
    // Fixed size buffer to avoid heap allocation
    // Overhead per byte: Start(2 cmds) + 8*Data(2 cmds) + Stop(2 cmds) = ~60 bytes of MPSSE commands
    // 4096 bytes buffer can hold commands for ~68 bytes of data, which is sufficient for small chunks.
    std::array<uint8_t, 4096> cmd_buffer;
    size_t cmd_idx = 0;

    auto flush_cmds = [&]() -> bool {
        if (cmd_idx == 0) return true;
        if (ftdi_write_data(&dev.ctx, cmd_buffer.data(), cmd_idx) < 0) {
            return false;
        }
        cmd_idx = 0;
        return true;
    };

    auto push_byte = [&](uint8_t b) -> bool {
        if (cmd_idx >= cmd_buffer.size()) {
            if (!flush_cmds()) return false;
        }
        cmd_buffer[cmd_idx++] = b;
        return true;
    };

    // 1. Set Baud Rate for UART (115200)
    // Command 0x86: Set TCK Divisor
    if (!push_byte(0x86)) return std::unexpected(UartError::WriteError);
    if (!push_byte(DIVISOR_115200 & 0xFF)) return std::unexpected(UartError::WriteError);
    if (!push_byte((DIVISOR_115200 >> 8) & 0xFF)) return std::unexpected(UartError::WriteError);

    // Helper to set TX pin state
    auto queue_tx = [&](bool high) -> bool {
        if (high) dev.acbus_val |= TX_PIN_MASK;
        else      dev.acbus_val &= ~TX_PIN_MASK;
        
        // 0x82: Set Data bits HighByte (ACBUS)
        if (!push_byte(0x82)) return false;
        if (!push_byte(dev.acbus_val)) return false;
        if (!push_byte(dev.acbus_dir)) return false;
        return true;
    };

    // Helper to wait 1 bit time
    // We use "Clock Bits Out" (0x13) with 0 data to create a delay of 1 bit period
    // at the configured baud rate. This toggles TCK/DO but CS is high so SPI ignores it.
    auto queue_delay = [&]() -> bool {
        if (!push_byte(0x13)) return false; // Clock Data Bits Out on -ve clock edge LSB first
        if (!push_byte(0)) return false;    // Length = 0 (1 bit)
        if (!push_byte(0x00)) return false; // Data
        return true;
    };

    // Ensure Line is Idle (High)
    if (!queue_tx(true)) return std::unexpected(UartError::WriteError);

    for (uint8_t byte : data) {
        // Start Bit (Low)
        if (!queue_tx(false)) return std::unexpected(UartError::WriteError);
        if (!queue_delay()) return std::unexpected(UartError::WriteError);

        // Data Bits (LSB First)
        for (int i = 0; i < 8; ++i) {
            if (!queue_tx(byte & (1 << i))) return std::unexpected(UartError::WriteError);
            if (!queue_delay()) return std::unexpected(UartError::WriteError);
        }

        // Stop Bit (High)
        if (!queue_tx(true)) return std::unexpected(UartError::WriteError);
        if (!queue_delay()) return std::unexpected(UartError::WriteError);
    }

    // Restore Baud Rate to 1MHz for SPI
    if (!push_byte(0x86)) return std::unexpected(UartError::WriteError);
    if (!push_byte(DIVISOR_1MHZ & 0xFF)) return std::unexpected(UartError::WriteError);
    if (!push_byte((DIVISOR_1MHZ >> 8) & 0xFF)) return std::unexpected(UartError::WriteError);

    if (!flush_cmds()) {
        return std::unexpected(UartError::WriteError);
    }

    return {};
}

} // namespace system1::hal