#include "hal/gpio.hpp"
#include "ftdi_context.hpp"

namespace system1::hal
{

GpioPin::GpioPin(int pin_id, Mode mode) : id_(pin_id)
{
    auto& dev = get_ftdi_instance();
    if (!dev.ensure_connected()) return;

    bool is_adbus = (pin_id < 8);
    int bit = is_adbus ? pin_id : (pin_id - 8);
    uint8_t mask = (1 << bit);

    if (is_adbus) {
        if (mode == Mode::Input) {
            dev.adbus_dir &= ~mask;
        } else {
            dev.adbus_dir |= mask;
        }
        dev.flush_gpio(true);
    } else {
        if (mode == Mode::Input) {
            dev.acbus_dir &= ~mask;
        } else {
            dev.acbus_dir |= mask;
        }
        dev.flush_gpio(false);
    }
}

void GpioPin::set(State state)
{
    auto& dev = get_ftdi_instance();
    bool is_adbus = (id_ < 8);
    int bit = is_adbus ? id_ : (id_ - 8);
    uint8_t mask = (1 << bit);
    uint8_t val = (state == State::High) ? mask : 0;

    if (is_adbus) {
        dev.adbus_val = (dev.adbus_val & ~mask) | val;
        dev.flush_gpio(true);
    } else {
        dev.acbus_val = (dev.acbus_val & ~mask) | val;
        dev.flush_gpio(false);
    }
}

GpioPin::State GpioPin::get() const
{
    auto& dev = get_ftdi_instance();
    bool is_adbus = (id_ < 8);
    int bit = is_adbus ? id_ : (id_ - 8);
    uint8_t mask = (1 << bit);

    // For output pins, return cached value to avoid USB roundtrip
    if (is_adbus && (dev.adbus_dir & mask)) {
        return (dev.adbus_val & mask) ? State::High : State::Low;
    }
    if (!is_adbus && (dev.acbus_dir & mask)) {
        return (dev.acbus_val & mask) ? State::High : State::Low;
    }

    // For input pins, read from device
    uint8_t cmd = is_adbus ? 0x81 : 0x83; // Get Data bits LowByte : HighByte
    uint8_t val = 0;
    
    if (ftdi_write_data(&dev.ctx, &cmd, 1) == 1) {
        ftdi_read_data(&dev.ctx, &val, 1);
    }
    return (val & mask) ? State::High : State::Low;
}

void GpioPin::set_pwm(float duty_cycle)
{
    // MPSSE cannot do hardware PWM on GPIOs concurrently with SPI easily.
    // We simulate simple on/off based on threshold for the host simulation.
    set(duty_cycle > 0.5f ? State::High : State::Low);
}

} // namespace system1::hal