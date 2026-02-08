#ifndef MODULES_HAL_BLE_HPP
#define MODULES_HAL_BLE_HPP

#include <cstdint>
#include <span>
#include <expected>

namespace system1::hal
{

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
    
    bool is_connected() const;
};

} // namespace system1::hal

#endif // MODULES_HAL_BLE_HPP