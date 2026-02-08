#pragma once

#include <ftdi.h>
#include <cstdint>
#include "hal/logger.hpp"

namespace system1::hal
{

// Singleton wrapper for the FTDI device context to manage resource lifetime
// and ensure single access to the hardware resource on the host.
class FtdiContext {
public:
    struct ftdi_context ctx;
    bool is_open = false;
    
    // Cache for GPIO state to allow read-modify-write
    // ADBUS: SPI (0-3) + GPIO (4-7)
    // ACBUS: GPIO (0-7) -> Mapped to Pin ID 8-15
    uint8_t adbus_val = 0x08; // Initial: CS=1 (Bit 3), others 0
    uint8_t adbus_dir = 0xFB; // Initial: 1111 1011 (In: MISO/Bit2, Out: Others)
    
    uint8_t acbus_val = 0x00;
    uint8_t acbus_dir = 0x00; // All inputs by default

    FtdiContext() {
        if (ftdi_init(&ctx) < 0) {
            hal::log(LogLevel::Error, "FTDI", "Failed to initialize FTDI context");
        }
    }

    ~FtdiContext() {
        if (is_open) {
            ftdi_usb_close(&ctx);
        }
        ftdi_deinit(&ctx);
    }
    
    FtdiContext(const FtdiContext&) = delete;
    FtdiContext& operator=(const FtdiContext&) = delete;

    bool ensure_connected() {
        if (is_open) return true;
        
        ftdi_set_interface(&ctx, INTERFACE_A);
        
        if (ftdi_usb_open(&ctx, 0x0403, 0x6014) < 0) {
            if (ftdi_usb_open(&ctx, 0x0403, 0x6010) < 0) {
                hal::log(LogLevel::Error, "FTDI", "Failed to open FTDI device: {}", ftdi_get_error_string(&ctx));
                return false;
            }
        }
        
        is_open = true;
        ftdi_usb_reset(&ctx);
        ftdi_set_bitmode(&ctx, 0, BITMODE_MPSSE);
        
        // MPSSE Setup: 60MHz, Disable Div 5, Turn off adaptive clocking
        const uint8_t setup_cmds[] = { 0x8A, 0x97, 0x8D };
        ftdi_write_data(&ctx, const_cast<uint8_t*>(setup_cmds), sizeof(setup_cmds));
        
        // Apply initial GPIO state
        flush_gpio(true);  // ADBUS
        flush_gpio(false); // ACBUS
        
        return true;
    }

    void flush_gpio(bool low_byte) {
        uint8_t cmd[3];
        cmd[0] = low_byte ? 0x80 : 0x82; // Set Data bits LowByte : HighByte
        cmd[1] = low_byte ? adbus_val : acbus_val;
        cmd[2] = low_byte ? adbus_dir : acbus_dir;
        ftdi_write_data(&ctx, cmd, 3);
    }
};

inline FtdiContext& get_ftdi_instance() {
    static FtdiContext instance;
    return instance;
}

} // namespace system1::hal