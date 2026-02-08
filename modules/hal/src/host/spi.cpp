#include "hal/spi.hpp"
#include "ftdi_context.hpp"
#include <algorithm>

namespace system1::hal
{

namespace {
    // Helper to manage Chip Select (CS) lifetime
    struct ScopedChipSelect {
        FtdiContext& dev;
        
        ScopedChipSelect(FtdiContext& d) : dev(d) {
            // Chip Select Low (Active)
            // ADBUS3 is assumed to be CS
            dev.adbus_val &= ~0x08; 
            dev.flush_gpio(true);   
        }

        ~ScopedChipSelect() {
            // Chip Select High (Inactive)
            dev.adbus_val |= 0x08; 
            dev.flush_gpio(true);
        }
        
        // Prevent copying
        ScopedChipSelect(const ScopedChipSelect&) = delete;
        ScopedChipSelect& operator=(const ScopedChipSelect&) = delete;
    };
}

std::expected<void, SpiError> SpiMaster::transfer(std::span<const std::uint8_t> tx_data, std::span<std::uint8_t> rx_data)
{
    auto& dev = get_ftdi_instance();
    if (!dev.ensure_connected()) {
        return std::unexpected(SpiError::BusError);
    }

    size_t len = 0;
    if (!tx_data.empty() && !rx_data.empty()) {
        len = std::min(tx_data.size(), rx_data.size());
    } else if (!tx_data.empty()) {
        len = tx_data.size();
    } else if (!rx_data.empty()) {
        len = rx_data.size();
    }
    
    if (len == 0) return {};

    // RAII for Chip Select
    ScopedChipSelect cs(dev);

    size_t offset = 0;
    while (offset < len) {
        // MPSSE max length per command is 65536 bytes
        // We use 65535 to fit in uint16_t length parameter (Length - 1)
        size_t chunk_size = std::min(len - offset, (size_t)65535);
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
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) return std::unexpected(SpiError::BusError);
            if (ftdi_write_data(&dev.ctx, const_cast<uint8_t*>(tx_data.data() + offset), chunk_size) != (int)chunk_size) {
                return std::unexpected(SpiError::BusError);
            }
            
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
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) return std::unexpected(SpiError::BusError);
            if (ftdi_write_data(&dev.ctx, const_cast<uint8_t*>(tx_data.data() + offset), chunk_size) != (int)chunk_size) {
                return std::unexpected(SpiError::BusError);
            }
        } 
        else if (!rx_data.empty()) {
            // RX Only
            cmd_buf[0] = 0x20; // Bytes In +ve
            if (ftdi_write_data(&dev.ctx, cmd_buf, 3) != 3) return std::unexpected(SpiError::BusError);
            
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

    return {};
}

} // namespace system1::hal