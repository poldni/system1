#include "hal/logger.hpp"
#include <esp_log.h>

namespace system1::hal
{

void log_write(LogLevel level, std::string_view tag, std::string_view message, const std::source_location& /*loc*/)
{
    esp_log_level_t esp_level = ESP_LOG_INFO;
    switch (level) {
        case LogLevel::Debug:   esp_level = ESP_LOG_DEBUG; break;
        case LogLevel::Info:    esp_level = ESP_LOG_INFO; break;
        case LogLevel::Warning: esp_level = ESP_LOG_WARN; break;
        case LogLevel::Error:   esp_level = ESP_LOG_ERROR; break;
    }

    // We use "HAL" as the primary tag for ESP_LOG system to ensure it's enabled,
    // and print the specific component tag in the message.
    // %.*s is used to print string_view which might not be null-terminated.
    esp_log_write(esp_level, "HAL", "%.*s: %.*s\n", 
                  static_cast<int>(tag.size()), tag.data(),
                  static_cast<int>(message.size()), message.data());
}

} // namespace system1::hal