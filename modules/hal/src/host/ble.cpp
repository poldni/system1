#include "hal/ble.hpp"
#include "hal/logger.hpp"

namespace system1::hal
{

BleSender::BleSender()
{
    hal::log(LogLevel::Info, "BLE", "Host BLE Sender Initialized");
}

std::expected<void, BleError> BleSender::send(std::span<const std::uint8_t> data)
{
    if (data.empty()) {
        hal::log(LogLevel::Debug, "BLE", "TX 0 bytes");
    } else {
        hal::log(LogLevel::Debug, "BLE", "TX {} bytes: [{:02x}...]", data.size(), static_cast<unsigned>(data[0]));
    }
             
    return {};
}

bool BleSender::is_connected() const
{
    return true; // Always simulate connected on host
}

} // namespace system1::hal