#include "hal/uart.hpp"
#include "ftdi_context.hpp"
#include <vector>
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
    
    // Prepare command buffer
    // Overhead per byte: Start(2 cmds) + 8*Data(2 cmds) + Stop(2 cmds) = ~20 bytes of MPSSE commands
    std::vector<uint8_t> cmds;
    cmds.reserve(20 + data.size() * 25);

    // 1. Set Baud Rate for UART (115200)
    // Command 0x86: Set TCK Divisor
    cmds.push_back(0x86); 
    cmds.push_back(DIVISOR_115200 & 0xFF);
    cmds.push_back((DIVISOR_115200 >> 8) & 0xFF);

    // Helper to set TX pin state
    auto queue_tx = [&](bool high) {
        if (high) dev.acbus_val |= TX_PIN_MASK;
        else      dev.acbus_val &= ~TX_PIN_MASK;
        
        // 0x82: Set Data bits HighByte (ACBUS)
        cmds.push_back(0x82);
        cmds.push_back(dev.acbus_val);
        cmds.push_back(dev.acbus_dir);
    };

    // Helper to wait 1 bit time
    // We use "Clock Bits Out" (0x13) with 0 data to create a delay of 1 bit period
    // at the configured baud rate. This toggles TCK/DO but CS is high so SPI ignores it.
    auto queue_delay = [&]() {
        cmds.push_back(0x13); // Clock Data Bits Out on -ve clock edge LSB first
        cmds.push_back(0);    // Length = 0 (1 bit)
        cmds.push_back(0x00); // Data
    };

    // Ensure Line is Idle (High)
    queue_tx(true);

    for (uint8_t byte : data) {
        // Start Bit (Low)
        queue_tx(false);
        queue_delay();

        // Data Bits (LSB First)
        for (int i = 0; i < 8; ++i) {
            queue_tx(byte & (1 << i));
            queue_delay();
        }

        // Stop Bit (High)
        queue_tx(true);
        queue_delay();
    }

    // Restore Baud Rate to 1MHz for SPI
    cmds.push_back(0x86);
    cmds.push_back(DIVISOR_1MHZ & 0xFF);
    cmds.push_back((DIVISOR_1MHZ >> 8) & 0xFF);

    if (ftdi_write_data(&dev.ctx, cmds.data(), cmds.size()) < 0) {
        return std::unexpected(UartError::WriteError);
    }

    return {};
}

} // namespace system1::hal