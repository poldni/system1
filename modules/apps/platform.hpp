#ifndef APP_PLATFORM_HPP
#define APP_PLATFORM_HPP

#include "hal/spi.hpp"
#include "hal/uart.hpp"
#include "hal/ble.hpp"
#include "hal/gpio.hpp"

namespace system1::app
{

// Platform-specific Pin Definitions
// These constants map abstract functions to physical pins based on the build target.
#if defined(IDF_TARGET)
    // ESP32-C6 Target
    // Note: SPI/UART pins are defined internally in their respective HAL implementation files
    // or via Kconfig, but GPIOs for simple peripherals are defined here.
    constexpr int PIN_LED_RED   = 8;
    constexpr int PIN_LED_RGB_R = 20;
    constexpr int PIN_LED_RGB_G = 21;
    constexpr int PIN_LED_RGB_B = 22;
    constexpr int PIN_BUZZER    = 12;
    constexpr int PIN_BUTTON    = 9;
#else
    // Host Target (e.g., FTDI MPSSE on Windows/Linux)
    // Mapping corresponds to FTDI ADBUS/ACBUS lines
    constexpr int PIN_LED_RED   = 4;  // ADBUS4
    constexpr int PIN_LED_RGB_R = 5;  // ADBUS5
    constexpr int PIN_LED_RGB_G = 6;  // ADBUS6
    constexpr int PIN_LED_RGB_B = 7;  // ADBUS7
    constexpr int PIN_BUZZER    = 9;  // ACBUS1
    constexpr int PIN_BUTTON    = 10; // ACBUS2
#endif

/**
 * @brief Platform abstraction that aggregates hardware resources.
 * 
 * This class instantiates and holds references to the concrete HAL drivers.
 * It ensures that hardware resources are initialized in a deterministic order
 * (declaration order) and provides accessors for dependency injection into
 * application components like DataProcessor.
 */
class Platform
{
public:
    Platform()
        : spi_()
        , uart_()
        , ble_()
        , led_red_(PIN_LED_RED, hal::GpioPin::Mode::Pwm)
        , led_rgb_r_(PIN_LED_RGB_R, hal::GpioPin::Mode::Pwm)
        , led_rgb_g_(PIN_LED_RGB_G, hal::GpioPin::Mode::Pwm)
        , led_rgb_b_(PIN_LED_RGB_B, hal::GpioPin::Mode::Pwm)
        , buzzer_(PIN_BUZZER, hal::GpioPin::Mode::Pwm)
        , button_(PIN_BUTTON, hal::GpioPin::Mode::Input)
    {
    }

    // Delete copy/move to ensure single instance semantics and hardware exclusivity
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

    // Accessors for HAL instances
    // These return references to the concrete driver types, which satisfy
    // the concepts required by DataProcessor (SpiDriverConcept, BleDriverConcept).
    
    hal::SpiMaster& spi() { return spi_; }
    hal::UartDevice& uart() { return uart_; }
    hal::BleSender& ble() { return ble_; }
    
    hal::GpioPin& led_red() { return led_red_; }
    hal::GpioPin& led_rgb_r() { return led_rgb_r_; }
    hal::GpioPin& led_rgb_g() { return led_rgb_g_; }
    hal::GpioPin& led_rgb_b() { return led_rgb_b_; }
    hal::GpioPin& buzzer() { return buzzer_; }
    hal::GpioPin& button() { return button_; }

private:
    // Hardware Drivers
    // Order of declaration determines order of initialization.
    hal::SpiMaster spi_;
    hal::UartDevice uart_;
    hal::BleSender ble_;
    
    // GPIO Peripherals
    hal::GpioPin led_red_;
    hal::GpioPin led_rgb_r_;
    hal::GpioPin led_rgb_g_;
    hal::GpioPin led_rgb_b_;
    hal::GpioPin buzzer_;
    hal::GpioPin button_;
};

} // namespace system1::app

#endif // APP_PLATFORM_HPP
