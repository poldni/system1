#ifndef MODULES_HAL_BLE_HPP
#define MODULES_HAL_BLE_HPP

#include <cstdint>
#include <span>
#include <expected>
#include <optional>

namespace system1::hal
{

struct __attribute__((packed)) DeviceSettings {
    uint8_t sensitivity;
    uint8_t led_brightness;
    uint16_t reporting_interval_ms;
};

enum class BleError
{
    None,
    NotConnected,
    TransmissionFailed,
    InternalError
};

class BleSender
{
public:
    BleSender();
    
    // Send data via BLE Notification
    std::expected<void, BleError> send(std::span<const std::uint8_t> data);

    // Check for pending settings updates received from the client
    std::optional<DeviceSettings> get_pending_settings();
    
    bool is_connected() const;
};

} // namespace system1::hal

#endif // MODULES_HAL_BLE_HPP