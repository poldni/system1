#include "hal/spi.hpp"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <algorithm>

namespace system1::hal
{

namespace 
{
    // ESP32-C6 FSPI (SPI2) IOMUX pins for optimal performance
    constexpr gpio_num_t PIN_NUM_MISO = GPIO_NUM_2;
    constexpr gpio_num_t PIN_NUM_MOSI = GPIO_NUM_7;
    constexpr gpio_num_t PIN_NUM_CLK  = GPIO_NUM_6;
    constexpr gpio_num_t PIN_NUM_CS   = GPIO_NUM_10; // Arbitrary free pin for CS

    // Wrapper class to manage the lifetime and initialization of the SPI driver.
    // This allows us to use the default constructor of SpiMaster in the header
    // while performing the necessary hardware initialization lazily or statically.
    class EspSpiDriver {
    public:
        spi_device_handle_t handle = nullptr;

        EspSpiDriver() {
            // Configuration for the SPI bus
            spi_bus_config_t buscfg = {};
            buscfg.mosi_io_num = PIN_NUM_MOSI;
            buscfg.miso_io_num = PIN_NUM_MISO;
            buscfg.sclk_io_num = PIN_NUM_CLK;
            buscfg.quadwp_io_num = -1;
            buscfg.quadhd_io_num = -1;
            // Max transfer size in bytes. 
            // Note: If buffers are not DMA-capable or not 4-byte aligned, 
            // the driver might allocate temporary internal buffers.
            buscfg.max_transfer_sz = 4096;

            // Initialize the SPI bus
            // Using SPI2_HOST (FSPI) which is available on C6
            // SPI_DMA_CH_AUTO selects the best available DMA channel
            if (spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
                return;
            }

            // Configuration for the SPI device on the other side of the bus
            spi_device_interface_config_t devcfg = {};
            devcfg.clock_speed_hz = 1 * 1000 * 1000;           // 1 MHz Clock
            devcfg.mode = 0;                                  // SPI mode 0
            devcfg.spics_io_num = PIN_NUM_CS;                 // CS pin
            devcfg.queue_size = 1;                            // Transaction queue size
            
            // Attach the device to the SPI bus
            spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
        }

        // Delete copy/move to enforce singleton usage
        EspSpiDriver(const EspSpiDriver&) = delete;
        EspSpiDriver& operator=(const EspSpiDriver&) = delete;
    };

    // Singleton instance of the driver wrapper
    EspSpiDriver& get_driver() {
        static EspSpiDriver driver;
        return driver;
    }
}

std::expected<void, SpiError> SpiMaster::transfer(std::span<const std::uint8_t> tx_data, std::span<std::uint8_t> rx_data)
{
    auto& driver = get_driver();
    if (!driver.handle) {
        return std::unexpected(SpiError::BusError);
    }

    if (tx_data.empty() && rx_data.empty()) {
        return {};
    }

    size_t length = 0;
    if (!tx_data.empty()) length = tx_data.size();
    if (!rx_data.empty()) length = std::max(length, rx_data.size());

    spi_transaction_t t = {};
    t.length = length * 8; // Transaction length is in bits
    t.tx_buffer = tx_data.empty() ? nullptr : tx_data.data();
    t.rx_buffer = rx_data.empty() ? nullptr : rx_data.data();

    // Perform the transaction
    // spi_device_transmit is blocking and handles the transfer
    esp_err_t ret = spi_device_transmit(driver.handle, &t);

    if (ret != ESP_OK) {
        if (ret == ESP_ERR_TIMEOUT) return std::unexpected(SpiError::Timeout);
        return std::unexpected(SpiError::BusError);
    }

    return {};
}

} // namespace system1::hal