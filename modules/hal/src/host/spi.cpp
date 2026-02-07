#include "hal/spi.hpp"
#include "ftdi_context.hpp"
#include <algorithm>
#include <vector>

namespace system1::hal
{

std::expected<void, SpiError> SpiMaster::transfer(std::span<const std::uint8_t> tx_data, std::span<std::uint8_t> rx_data)
{
    auto& dev = get_ftdi_instance();
    if (!dev.ensure_connected()) {
        return std::unexpected(SpiError::BusError);
    }

    size_t len = 0;
    if (!tx_data.empty()) len = tx_data.size();
    if (!rx_data.empty()) len = std::max(len, rx_data.size());
    
    if (len == 0) return {};

    // Chip Select Low
    // We modify the cached state to preserve other GPIOs on ADBUS
    dev.adbus_val &= ~0x08; // Clear bit 3 (CS)
    dev.flush_gpio(true);   // Flush ADBUS

    size_t offset = 0;
    while (offset < len) {
        // MPSSE max length per command is 65536 bytes
        size_t chunk_size = std::min(len - offset, (size_t)65536);
        uint16_t len_param = static_cast<uint16_t>(chunk_size - 1);
        
        uint8_t cmd_buf[3];
        cmd_buf[1] = len_param & 0xFF;
        cmd_buf[2] = (len_param >> 8) & 0xFF;

        // Determine MPSSE command based on available buffers
        // Mode 0: CPHA=0, CPOL=0. 
        // MPSSE: Data Out on -ve edge, Data In on +ve edge. MSB First.
        
        if (!tx_data.empty() && !rx_data.empty()) {
            // Full Duplex
            cmd_buf[0] = 0x31; // Bytes Out -ve, Bytes In +ve
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) break;
            if (ftdi_write_data(&dev.ctx, const_cast<uint8_t*>(tx_data.data() + offset), chunk_size) != (int)chunk_size) break;
            
            // Read back
            size_t read_total = 0;
            while (read_total < chunk_size) {
                int ret = ftdi_read_data(&dev.ctx, rx_data.data() + offset + read_total, chunk_size - read_total);
                if (ret < 0) return std::unexpected(SpiError::Timeout);
                read_total += ret;
            }
        } 
        else if (!tx_data.empty()) {
            // TX Only
            cmd_buf[0] = 0x11; // Bytes Out -ve
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) break;
            if (ftdi_write_data(&dev.ctx, const_cast<uint8_t*>(tx_data.data() + offset), chunk_size) != (int)chunk_size) break;
        } 
        else if (!rx_data.empty()) {
            // RX Only
            cmd_buf[0] = 0x20; // Bytes In +ve
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) break;
            
            // Read back
            size_t read_total = 0;
            while (read_total < chunk_size) {
                int ret = ftdi_read_data(&dev.ctx, rx_data.data() + offset + read_total, chunk_size - read_total);
                if (ret < 0) return std::unexpected(SpiError::Timeout);
                read_total += ret;
            }
        }
        
        offset += chunk_size;
    }

    // Chip Select High
    dev.adbus_val |= 0x08; // Set bit 3 (CS)
    dev.flush_gpio(true);

    return {};
}

} // namespace system1::hal