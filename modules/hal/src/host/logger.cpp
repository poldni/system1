#include "hal/logger.hpp"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace system1::hal
{

namespace
{
    std::mutex log_mutex;
}

void log_write(LogLevel level, std::string_view tag, std::string_view message, const std::source_location& loc)
{
    std::lock_guard lock(log_mutex);

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::ostream& stream = (level == LogLevel::Error) ? std::cerr : std::cout;

    char level_char = 'I';
    switch (level) {
        case LogLevel::Debug:   level_char = 'D'; break;
        case LogLevel::Info:    level_char = 'I'; break;
        case LogLevel::Warning: level_char = 'W'; break;
        case LogLevel::Error:   level_char = 'E'; break;
    }

    stream << "[" << std::put_time(std::localtime(&time), "%T") << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
           << level_char << " (" << tag << ") " << message 
           << " [" << loc.file_name() << ":" << loc.line() << "]" << std::endl;
}

} // namespace system1::hal