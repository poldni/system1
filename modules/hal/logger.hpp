#ifndef MODULES_HAL_LOGGER_HPP
#define MODULES_HAL_LOGGER_HPP

#include <string_view>
#include <source_location>
#include <format>
#include <array>
#include <type_traits>
#include <iterator>

namespace system1::hal
{

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

/**
 * @brief Internal implementation to write the log message to the backend.
 * Implemented differently for ESP32 and Host.
 */
void log_write(LogLevel level, std::string_view tag, std::string_view message, const std::source_location& loc);

/**
 * @brief Helper iterator to write to a fixed-size buffer.
 */
struct BoundedLogIterator {
    char* dest;
    size_t remaining;

    using iterator_category = std::output_iterator_tag;
    using value_type = void;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = void;

    BoundedLogIterator& operator=(char c) {
        if (remaining > 0) {
            *dest++ = c;
            --remaining;
        }
        return *this;
    }
    BoundedLogIterator& operator*() { return *this; }
    BoundedLogIterator& operator++() { return *this; }
    BoundedLogIterator operator++(int) { return *this; }
};

/**
 * @brief Helper struct to capture source location with format string.
 * This avoids ambiguity in template argument deduction when source_location is the last argument.
 */
struct LogFormat {
    std::string_view fmt;
    std::source_location loc;

    LogFormat(const char* f, const std::source_location& l = std::source_location::current())
        : fmt(f), loc(l) {}
    LogFormat(std::string_view f, const std::source_location& l = std::source_location::current())
        : fmt(f), loc(l) {}
};

/**
 * @brief Thread-safe logger using C++23 std::format.
 * 
 * Uses a stack buffer to avoid dynamic allocation for the formatted string.
 */
template <typename... Args>
void log(LogLevel level, std::string_view tag, LogFormat format, Args&&... args)
{
    if constexpr (sizeof...(Args) == 0) {
        log_write(level, tag, format.fmt, format.loc);
    } else {
        // 256 bytes buffer for log messages to avoid heap allocation
        std::array<char, 256> buffer;
        BoundedLogIterator it{buffer.data(), buffer.size() - 1};
        auto end_it = std::vformat_to(it, format.fmt, std::make_format_args(args...));
        *end_it.dest = '\0';
        
        log_write(level, tag, std::string_view(buffer.data(), end_it.dest - buffer.data()), format.loc);
    }
}

} // namespace system1::hal

#endif // MODULES_HAL_LOGGER_HPP