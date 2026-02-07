#include "hal/uart.hpp"
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace system1::hal
{

namespace 
{
    // Configuration for UART1 (UART0 is typically used for console/logging)
    constexpr uart_port_t UART_PORT = UART_NUM_1;
    constexpr int TX_BUF_SIZE = 1024;
    constexpr int RX_BUF_SIZE = 1024;
    
    // Arbitrary pins for UART1 on ESP32-C6
    constexpr gpio_num_t TXD_PIN = GPIO_NUM_4;
    constexpr gpio_num_t RXD_PIN = GPIO_NUM_5;

    // Singleton wrapper for UART driver lifetime management to ensure
    // initialization happens once and resources are statically managed.
    class EspUartDriver {
    public:
        bool initialized = false;

        EspUartDriver() {
            const uart_config_t uart_config = {
                .baud_rate = 115200,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                .rx_flow_ctrl_thresh = 122,
                .source_clk = UART_SCLK_DEFAULT,
            };

            // Install UART driver
            // We do not use an event queue here as we use blocking/polling reads
            if (uart_driver_install(UART_PORT, RX_BUF_SIZE, TX_BUF_SIZE, 0, nullptr, 0) != ESP_OK) {
                return;
            }

            if (uart_param_config(UART_PORT, &uart_config) != ESP_OK) {
                return;
            }

            if (uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
                return;
            }

            initialized = true;
        }

        // Delete copy/move to enforce singleton usage
        EspUartDriver(const EspUartDriver&) = delete;
        EspUartDriver& operator=(const EspUartDriver&) = delete;
    };

    EspUartDriver& get_uart_driver() {
        static EspUartDriver driver;
        return driver;
    }
}

std::expected<std::size_t, UartError> UartDevice::read(std::span<std::uint8_t> buffer)
{
    auto& driver = get_uart_driver();
    if (!driver.initialized) {
        return std::unexpected(UartError::ReadError);
    }

    if (buffer.empty()) {
        return 0;
    }

    // Read with a timeout slightly larger than the expected data rate (70ms)
    // to allow for processing jitter.
    const TickType_t timeout = pdMS_TO_TICKS(100);
    
    // uart_read_bytes returns int, -1 on error
    int len = uart_read_bytes(UART_PORT, buffer.data(), buffer.size(), timeout);

    if (len < 0) {
        return std::unexpected(UartError::ReadError);
    }

    return static_cast<std::size_t>(len);
}

std::expected<void, UartError> UartDevice::write(std::span<const std::uint8_t> data)
{
    auto& driver = get_uart_driver();
    if (!driver.initialized) {
        return std::unexpected(UartError::WriteError);
    }

    if (data.empty()) {
        return {};
    }

    int len = uart_write_bytes(UART_PORT, data.data(), data.size());

    if (len < 0) {
        return std::unexpected(UartError::WriteError);
    }

    return {};
}

} // namespace system1::hal